#include <GLES3/gl32.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "modules/bar/panel/resource_panel.h"

#include "render/arc_gauge.h"
#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/palette.h"
#include "render/panel_scroll.h"
#include "render/progress_bar.h"
#include "render/text.h"

constexpr Color kGaugeColorCpu = color(kGaugeColorCpuHex);
constexpr Color kGaugeColorGpu = color(kGaugeColorGpuHex);
constexpr Color kGaugeColorRam = color(kGaugeColorRamHex);
constexpr Color kGaugeColorDisk = color(kGaugeColorDiskHex);
constexpr Color kTempWarnColor = color(kTempWarnColorHex);

using namespace panel_chrome_detail;

namespace resource_panel_detail {

namespace {

const Color &temp_color(float celsius, const Color &base) {
    if (celsius >= kTempCriticalCelsius)
        return palette::critical;
    if (celsius >= kTempWarnCelsius)
        return kTempWarnColor;
    return base;
}

const Color &usage_color(float value01, const Color &base) {
    if (value01 >= kUsageCriticalThreshold)
        return palette::critical;
    if (value01 >= kUsageWarnThreshold)
        return kTempWarnColor;
    return base;
}

float text_height(TextureCache &tcache, const std::string &text, int32_t scale) {
    const Texture *tex = cached_text(tcache, text, scale);
    return tex ? static_cast<float>(tex->height) : 0.0f;
}

float measure_device_content_h() {
    return 2 * kGaugeDiameter + kGaugeRowGap;
}

float measure_memory_content_h(TextureCache &tcache, int32_t scale, const SystemStatsState &stats) {
    int lines = stats.disk_pct >= 0.0f ? 2 : 1;
    float line_h = text_height(tcache, "RAM", scale) + kMemoryLabelBarGap + kMemoryBarHeight;
    return lines * line_h + (lines - 1) * kMemoryLineGap;
}

} // namespace

std::vector<PanelRow> build_rows(const CpuTempState &, const GpuTempState &, const SystemStatsState &stats, TextureCache &tcache, int32_t scale) {
    std::vector<PanelRow> rows;
    rows.push_back({RowKind::DeviceCards, panel_card_box_height(measure_device_content_h())});
    rows.push_back({RowKind::MemoryCard, panel_card_box_height(measure_memory_content_h(tcache, scale, stats))});
    rows.push_back({RowKind::Spacer, kPanelTrailingSpacerHeight});
    return rows;
}

float content_height(const std::vector<PanelRow> &rows) {
    float h = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0)
            h += kResourceCardGap;
        h += rows[i].height;
    }
    return h;
}

float panel_height(const std::vector<PanelRow> &rows) {
    float h = kPanelPadding + kPanelHeaderHeight + kPanelHeaderDividerGap + 1.0f + kPanelContentGap + content_height(rows) + kPanelPadding;
    return std::min(kResourcePanelMaxHeight, h);
}

} // namespace resource_panel_detail

bool resource_panel_create_surface(ResourcePanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell, "astralia-shell-resource-panel", output);
}

bool resource_panel_init_egl(ResourcePanelState &state, Renderer &renderer, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state, &cpu_temp, &gpu_temp, &stats] {
        resource_panel_paint(state, cpu_temp, gpu_temp, stats, state.pending_pill_center_x, state.pending_bar_height, state.pending_bar_top_margin);
    };
    return true;
}

void resource_panel_request_frame(ResourcePanelState &state, float pill_center_x, float bar_height, float bar_top_margin) {
    state.pending_pill_center_x = pill_center_x;
    state.pending_bar_height = bar_height;
    state.pending_bar_top_margin = bar_top_margin;
    overlay_panel_request_frame(state.base);
}

void resource_panel_toggle(ResourcePanelState &state, float pill_center_x) {
    panel_lock_toggle(state.base, state.locked_center_x, pill_center_x, [&state] { panel_reveal_open(state.reveal); }, [&state] {
            state.scroll_offset = 0.0f;
            panel_reveal_close(state.reveal, state.base, [&state] { state.locked_center_x = -1.0f; }); });
}

void resource_panel_handle_scroll(ResourcePanelState &state, const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, double dy) {
    using namespace resource_panel_detail;
    state.scroll_offset =
        panel_clamp_scroll(state.scroll_offset, static_cast<float>(dy), content_height(build_rows(cpu, gpu, stats, state.tcache, state.base.output_scale.scale)), state.visible_content_height);
}

void resource_panel_handle_click(ResourcePanelState &state, double px, double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        if (region.kind == PanelClickKind::Close)
            resource_panel_toggle(state);
        return;
    }

    if (!hit(state.panel_rect, px, py))
        resource_panel_toggle(state);
}

void resource_panel_handle_key_event(ResourcePanelState &state, const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        resource_panel_toggle(state);
}

using namespace resource_panel_detail;

namespace {

struct DeviceSpec {
    const char *title;
    float usage01;
    float clock_ghz;
    float celsius;
    const Color &base;
};

struct MemoryLine {
    const char *label;
    std::string detail;
    float value01;
    const Color *color;
};

std::string format_ghz(float ghz) {
    if (ghz < 0.0f)
        return "--";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f GHz", ghz);
    return buf;
}

std::string format_percent(float value01) {
    return value01 < 0.0f ? "--" : std::to_string(static_cast<int>(value01 * 100.0f)) + "%";
}

std::string format_celsius(float celsius) {
    return (celsius < 0.0f ? std::string("--") : std::to_string(static_cast<int>(celsius))) + "°C";
}

std::string format_used_cap(float used_gb, float total_gb) {
    if (used_gb < 0.0f || total_gb < 0.0f)
        return "--";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f / %.2f GB", used_gb, total_gb);
    return buf;
}

float draw_device_card(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const DeviceSpec &d) {
    PanelCardChrome chrome = panel_draw_card(root, tcache, scale, x, y, w, measure_device_content_h(), d.title);
    float content_w = w - 2 * kCardHorizontalPadding;
    float gauge_x = chrome.content_x + (content_w - kGaugeDiameter) / 2.0f;
    int center_max_w = static_cast<int>(kGaugeDiameter - 2 * kGaugeStroke);

    const Texture *clock_tex = cached_text_clipped(tcache, format_ghz(d.clock_ghz), scale, center_max_w);
    const Texture *usage_tex = cached_text_clipped(tcache, format_percent(d.usage01), scale, center_max_w);
    draw_arc_gauge(root, tcache, scale, gauge_x, chrome.content_y, kGaugeDiameter, kGaugeStroke, std::max(0.0f, d.usage01), usage_color(d.usage01, d.base), clock_tex, rgba(palette::text_dim), usage_tex, rgba(palette::text), nullptr, nullptr, kGaugeCenterLineGap, 0.0f);

    const Texture *temp_tex = cached_text_clipped(tcache, format_celsius(d.celsius), scale, center_max_w);
    float temp_y = chrome.content_y + kGaugeDiameter + kGaugeRowGap;
    draw_arc_gauge(root, tcache, scale, gauge_x, temp_y, kGaugeDiameter, kGaugeStroke, std::clamp(d.celsius / kTempGaugeMaxCelsius, 0.0f, 1.0f), temp_color(d.celsius, d.base), nullptr, nullptr, temp_tex, rgba(palette::text), nullptr, nullptr, 0.0f, 0.0f);
    return chrome.box_h;
}

float draw_device_cards(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats) {
    float card_w = (w - kResourceCardGap) / 2.0f;
    DeviceSpec cpu{"CPU", stats.cpu_usage, stats.cpu_freq_ghz, cpu_temp_available(cpu_temp) ? cpu_temp.celsius : -1.0f, kGaugeColorCpu};
    DeviceSpec gpu{"GPU", gpu_temp.usage_percent < 0.0f ? -1.0f : gpu_temp.usage_percent / 100.0f, gpu_temp.clock_ghz, gpu_temp_available(gpu_temp) ? gpu_temp.celsius : -1.0f, kGaugeColorGpu};
    draw_device_card(root, tcache, scale, x, y, card_w, cpu);
    return draw_device_card(root, tcache, scale, x + card_w + kResourceCardGap, y, card_w, gpu);
}

float draw_memory_card(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const SystemStatsState &stats) {
    PanelCardChrome chrome = panel_draw_card(root, tcache, scale, x, y, w, measure_memory_content_h(tcache, scale, stats), "Memory");
    float content_w = w - 2 * kCardHorizontalPadding;

    std::vector<MemoryLine> lines;
    lines.push_back({"RAM", format_used_cap(stats.mem_used_gb, stats.mem_total_gb), stats.mem_usage, &usage_color(stats.mem_usage, kGaugeColorRam)});
    if (stats.disk_pct >= 0.0f) {
        float disk01 = std::clamp(stats.disk_pct / 100.0f, 0.0f, 1.0f);
        lines.push_back({"Disk", format_used_cap(stats.disk_used_gb, stats.disk_total_gb), disk01, &usage_color(disk01, kGaugeColorDisk)});
    }

    float line_y = chrome.content_y;
    for (const MemoryLine &line : lines) {
        const Texture *label_tex = cached_text(tcache, line.label, scale);
        const Texture *detail_tex = cached_text(tcache, line.detail, scale);
        float text_h = std::max(label_tex ? static_cast<float>(label_tex->height) : 0.0f, detail_tex ? static_cast<float>(detail_tex->height) : 0.0f);
        if (label_tex)
            node_add_texture(root, chrome.content_x, line_y, *label_tex, rgba(palette::text));
        if (detail_tex)
            node_add_texture(root, chrome.content_x + content_w - detail_tex->width, line_y, *detail_tex, rgba(palette::text_dim));
        float bar_y = line_y + text_h + kMemoryLabelBarGap;
        draw_flat_bar(root, chrome.content_x, bar_y, content_w, kMemoryBarHeight, kMemoryBarRadius, std::max(0.0f, line.value01), kMemoryBarMinFill, rgba(palette::text_alpha08), rgba(*line.color));
        line_y = bar_y + kMemoryBarHeight + kMemoryLineGap;
    }
    return chrome.box_h;
}

} // namespace

void resource_panel_paint(ResourcePanelState &state, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, float pill_center_x, float bar_height, float bar_top_margin) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    state.base.animations.tick(std::chrono::steady_clock::now());
    gl_make_current(state.base.egl_display, state.base.egl_surface, state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.click_regions.clear();
    state.panel_rect = {};
    state.scene.rebuild();

    if (!state.base.open) {
        state.scene.draw(*state.renderer);
        eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
        return;
    }

    Node *root = &state.scene.root;

    std::vector<PanelRow> rows = build_rows(cpu_temp, gpu_temp, stats, state.tcache, scale);
    float panel_w = kResourcePanelWidth;
    if (state.locked_center_x < 0.0f)
        state.locked_center_x = pill_center_x;
    float clip_h =
        panel_reveal_tick(state.reveal, state.base, panel_height(rows));
    float panel_h = std::max(0.0f, state.reveal.target);
    float panel_x = std::clamp(state.locked_center_x - panel_w / 2.0f, kPanelSideMargin, static_cast<float>(state.base.width) - panel_w - kPanelSideMargin);
    float panel_y = bar_height + bar_top_margin + kPanelGap;
    state.panel_rect = {panel_x, panel_y, panel_w, panel_h};

    panel_draw_box(root, panel_x, panel_y, panel_w, panel_h);
    float header_y = panel_y + kPanelPadding;
    panel_draw_header(root, state.tcache, scale, "Resource", panel_x, panel_y, panel_w, state.click_regions);

    float divider_y = header_y + kPanelHeaderHeight + kPanelHeaderDividerGap;
    node_add_rect(root, panel_x + kPanelPadding, divider_y, panel_w - 2 * kPanelPadding, 1.0f, rgba(palette::text_alpha06));

    PanelScrollRegion region =
        panel_scroll_region(panel_x, panel_y, panel_w, panel_h);
    float content_x = region.content_x;
    float content_w = region.content_w;
    float content_top = region.content_top;
    float content_bottom = region.content_bottom;

    state.visible_content_height = std::max(0.0f, content_bottom - content_top);

    Node *scroll_clip =
        node_add_group(root, panel_x, content_top, panel_w, std::max(0.0f, content_bottom - content_top), true);

    float y = content_top - state.scroll_offset;
    for (size_t i = 0; i < rows.size(); ++i) {
        const PanelRow &row = rows[i];
        if (i > 0)
            y += kResourceCardGap;
        float row_h = row.height;
        bool row_visible = y + row_h > content_top && y < content_bottom;
        if (!row_visible) {
            y += row_h;
            continue;
        }

        switch (row.kind) {
        case RowKind::DeviceCards:
            draw_device_cards(scroll_clip, state.tcache, scale, content_x - panel_x, y - content_top, content_w, cpu_temp, gpu_temp, stats);
            break;
        case RowKind::MemoryCard:
            draw_memory_card(scroll_clip, state.tcache, scale, content_x - panel_x, y - content_top, content_w, stats);
            break;
        case RowKind::Spacer:
            break;
        }
        y += row_h;
    }

    if (clip_h + 0.5f < panel_h) {
        ScopedClip clip(*state.renderer, panel_x, panel_y, panel_w, std::max(0.0f, clip_h));
        state.scene.draw(*state.renderer);
    } else {
        state.scene.draw(*state.renderer);
    }
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive())
        overlay_panel_request_frame(state.base);
}
