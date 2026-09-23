#include <GLES3/gl32.h>
#include <algorithm>
#include <cmath>

#include "modules/bar/panel/system_monitor_panel.h"

#include "render/arc_gauge.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/palette.h"
#include "render/panel_scroll.h"
#include "render/text.h"

constexpr Color kGaugeColorCpu = color(kGaugeColorCpuHex);
constexpr Color kGaugeColorGpu = color(kGaugeColorGpuHex);
constexpr Color kGaugeColorRam = color(kGaugeColorRamHex);
constexpr Color kGaugeColorDisk = color(kGaugeColorDiskHex);
constexpr Color kTempWarnColor = color(kTempWarnColorHex);

using namespace panel_chrome_detail;

namespace system_monitor_panel_detail {

namespace {

const Color &temp_color(float celsius) {
    if (celsius >= 85.0f)
        return palette::critical;
    if (celsius >= 70.0f)
        return kTempWarnColor;
    return palette::text;
}

float measure_resources_content_h(TextureCache &tcache, int32_t scale, const SystemStatsState &stats, const GpuTempState &gpu_temp) {
    bool show_gpu = gpu_stats_available(gpu_temp);
    bool show_disk = stats.disk_pct >= 0.0f;
    int gauge_count = 2 + (show_gpu ? 1 : 0) + (show_disk ? 1 : 0);
    const Texture *label_h_tex = cached_text(tcache, "CPU", scale);
    float label_h = label_h_tex ? static_cast<float>(label_h_tex->height) : 0.0f;
    float gauge_unit_h = kGaugeDiameter + kStatsGaugeLabelSpacing + label_h;
    int rows = (gauge_count + 1) / 2;
    return rows * gauge_unit_h + (rows - 1) * kGaugeColumnGap;
}

float measure_cpu_temp_content_h(TextureCache &tcache, int32_t scale, const CpuTempState &cpu_temp) {
    int core_count = 0;
    for (const CpuCoreTemp &core : cpu_temp.cores)
        if (core.celsius >= 0.0f)
            ++core_count;

    std::string headline =
        (cpu_temp_available(cpu_temp) ? std::to_string(static_cast<int>(cpu_temp.celsius)) : "--") +
        "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    float headline_h =
        std::max(headline_tex ? static_cast<float>(headline_tex->height) : 0.0f, kTempRowHeight);

    int rows = core_count == 0 ? 0 : (core_count + kCpuCoreColumns - 1) / kCpuCoreColumns;
    float grid_h = core_count == 0 ? 0.0f : kCpuTempGridTopMargin + rows * kCpuCoreItemHeight + std::max(0, rows - 1) * kCpuCoreRowSpacing;
    return headline_h + grid_h;
}

float measure_gpu_temp_content_h(TextureCache &tcache, int32_t scale, const GpuTempState &gpu_temp) {
    std::string headline = std::to_string(static_cast<int>(gpu_temp.celsius)) + "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    return std::max(headline_tex ? static_cast<float>(headline_tex->height) : 0.0f, kTempRowHeight);
}

} // namespace

std::vector<PanelRow> build_rows(const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, TextureCache &tcache, int32_t scale) {
    std::vector<PanelRow> rows;

    rows.push_back({RowKind::ResourcesCard, panel_card_box_height(measure_resources_content_h(tcache, scale, stats, gpu))});

    rows.push_back({RowKind::CpuTempCard, panel_card_box_height(measure_cpu_temp_content_h(tcache, scale, cpu))});

    if (gpu_temp_available(gpu))
        rows.push_back({RowKind::GpuTempCard, panel_card_box_height(measure_gpu_temp_content_h(tcache, scale, gpu))});

    rows.push_back({RowKind::Spacer, kPanelTrailingSpacerHeight});
    return rows;
}

float content_height(const std::vector<PanelRow> &rows) {
    float h = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0)
            h += kPanelListSpacing;
        h += rows[i].height;
    }
    return h;
}

float panel_height(const std::vector<PanelRow> &rows) {
    float h = kPanelPadding + kPanelHeaderHeight + kPanelHeaderDividerGap + 1.0f + kPanelContentGap + content_height(rows) + kPanelPadding;
    return std::min(kPanelMaxHeight, h);
}

} // namespace system_monitor_panel_detail

bool system_monitor_panel_create_surface(SystemMonitorPanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell, "astralia-shell-system-monitor-panel", output);
}

bool system_monitor_panel_init_egl(SystemMonitorPanelState &state, Renderer &renderer, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state, &cpu_temp, &gpu_temp, &stats] {
        system_monitor_panel_paint(state, cpu_temp, gpu_temp, stats, state.pending_pill_center_x, state.pending_bar_height, state.pending_bar_top_margin);
    };
    return true;
}

void system_monitor_panel_request_frame(SystemMonitorPanelState &state, float pill_center_x, float bar_height, float bar_top_margin) {
    state.pending_pill_center_x = pill_center_x;
    state.pending_bar_height = bar_height;
    state.pending_bar_top_margin = bar_top_margin;
    overlay_panel_request_frame(state.base);
}

void system_monitor_panel_toggle(SystemMonitorPanelState &state, float pill_center_x) {
    panel_lock_toggle(state.base, state.locked_center_x, pill_center_x, [&state] { panel_reveal_open(state.reveal); }, [&state] {
            state.scroll_offset = 0.0f;
            panel_reveal_close(state.reveal, state.base, [&state] { state.locked_center_x = -1.0f; }); });
}

void system_monitor_panel_handle_scroll(SystemMonitorPanelState &state, const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, double dy) {
    using namespace system_monitor_panel_detail;
    state.scroll_offset =
        panel_clamp_scroll(state.scroll_offset, static_cast<float>(dy), content_height(build_rows(cpu, gpu, stats, state.tcache, state.base.output_scale.scale)), state.visible_content_height);
}

void system_monitor_panel_handle_click(SystemMonitorPanelState &state, double px, double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        if (region.kind == PanelClickKind::Close)
            system_monitor_panel_toggle(state);
        return;
    }

    if (!hit(state.panel_rect, px, py))
        system_monitor_panel_toggle(state);
}

void system_monitor_panel_handle_key_event(SystemMonitorPanelState &state, const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        system_monitor_panel_toggle(state);
}

using namespace system_monitor_panel_detail;

namespace {

struct GaugeSpec {
    float value01;
    Color color;
    const char *icon_glyph;
    std::string value_label;
    const char *label;
};

float draw_resources_card(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const SystemStatsState &stats, const GpuTempState &gpu_temp, float origin_x, float origin_y) {
    x -= origin_x;
    y -= origin_y;

    const Texture *label_h_tex = cached_text(tcache, "CPU", scale);
    float label_h = label_h_tex ? static_cast<float>(label_h_tex->height) : 0.0f;
    float gauge_unit_h = kGaugeDiameter + kStatsGaugeLabelSpacing + label_h;
    float content_h = measure_resources_content_h(tcache, scale, stats, gpu_temp);

    PanelCardChrome chrome = panel_draw_card(root, tcache, scale, x, y, w, content_h, "Resources");
    float cx = chrome.content_x, cy = chrome.content_y;
    float content_w = w - 2 * kCardHorizontalPadding;
    float col_w = (content_w - kGaugeColumnGap) / 2.0f;

    std::vector<GaugeSpec> gauges;
    float cpu01 = std::max(0.0f, stats.cpu_usage);
    gauges.push_back({cpu01, kGaugeColorCpu, icon::cpu, stats.cpu_usage >= 0.0f ? std::to_string(static_cast<int>(cpu01 * 100.0f)) + "%" : "--", "CPU"});

    if (gpu_stats_available(gpu_temp)) {
        float gpu01 = std::max(0.0f, gpu_temp.usage_percent / 100.0f);
        gauges.push_back({gpu01, kGaugeColorGpu, icon::gpu, std::to_string(static_cast<int>(gpu_temp.usage_percent)) + "%", "GPU"});
    }

    float mem01 = std::max(0.0f, stats.mem_usage);
    gauges.push_back({mem01, kGaugeColorRam, icon::settings, stats.mem_usage >= 0.0f ? std::to_string(static_cast<int>(mem01 * 100.0f)) + "%" : "--", "RAM"});

    if (stats.disk_pct >= 0.0f) {
        float disk01 = std::clamp(stats.disk_pct / 100.0f, 0.0f, 1.0f);
        gauges.push_back({disk01, kGaugeColorDisk, icon::folder, std::to_string(static_cast<int>(stats.disk_pct)) + "%", "DISK"});
    }

    for (size_t i = 0; i < gauges.size(); ++i) {
        const GaugeSpec &g = gauges[i];
        int col = static_cast<int>(i) % 2;
        int row = static_cast<int>(i) / 2;
        float col_x = cx + col * (col_w + kGaugeColumnGap);
        float gauge_x = col_x + (col_w - kGaugeDiameter) / 2.0f;
        float gauge_y = cy + row * (gauge_unit_h + kGaugeColumnGap);

        const Texture *icon_tex = cached_icon(tcache, g.icon_glyph, scale);
        const Texture *value_tex = cached_text_clipped(tcache, g.value_label, scale, static_cast<int>(kGaugeDiameter));
        const Texture *sub_tex = cached_text(tcache, g.label, scale);
        draw_arc_gauge(root, tcache, scale, gauge_x, gauge_y, kGaugeDiameter, kGaugeStroke, g.value01, g.color, icon_tex, rgba(g.color), value_tex, rgba(palette::text), sub_tex, rgba(palette::text_dim), kGaugeIconValueGap, kStatsGaugeLabelSpacing);
    }

    return chrome.box_h;
}

float draw_cpu_temp_card(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const CpuTempState &cpu_temp, float origin_x, float origin_y) {
    x -= origin_x;
    y -= origin_y;

    std::vector<const CpuCoreTemp *> cores;
    for (const CpuCoreTemp &core : cpu_temp.cores)
        if (core.celsius >= 0.0f)
            cores.push_back(&core);

    std::string headline =
        (cpu_temp_available(cpu_temp) ? std::to_string(static_cast<int>(cpu_temp.celsius)) : "--") +
        "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    float headline_h =
        std::max(headline_tex ? static_cast<float>(headline_tex->height) : 0.0f, kTempRowHeight);

    float content_w = w - 2 * kCardHorizontalPadding;
    float cell_w = (content_w - (kCpuCoreColumns - 1) * kCpuCoreColumnSpacing) / kCpuCoreColumns;
    float content_h = measure_cpu_temp_content_h(tcache, scale, cpu_temp);

    PanelCardChrome chrome = panel_draw_card(root, tcache, scale, x, y, w, content_h, "CPU Temperature");
    float cx = chrome.content_x, cy = chrome.content_y;

    if (headline_tex)
        node_add_texture(root, cx, cy, *headline_tex, rgba(temp_color(cpu_temp.celsius)));

    float grid_y = cy + headline_h + kCpuTempGridTopMargin;
    for (size_t i = 0; i < cores.size(); ++i) {
        const CpuCoreTemp &core = *cores[i];
        int col = static_cast<int>(i) % kCpuCoreColumns;
        int row = static_cast<int>(i) / kCpuCoreColumns;
        float cell_x = cx + col * (cell_w + kCpuCoreColumnSpacing);
        float cell_y = grid_y + row * (kCpuCoreItemHeight + kCpuCoreRowSpacing);
        node_add_rrect(root, cell_x, cell_y, cell_w, kCpuCoreItemHeight, kCpuCoreItemRadius, 0.0f, rgba(palette::text_alpha08), kPanelNoBorder);

        const Texture *name_tex =
            cached_text(tcache, "#" + std::to_string(i), scale);
        if (name_tex)
            node_add_texture(root, cell_x + kCpuCoreTextMargin, cell_y + (kCpuCoreItemHeight - name_tex->height) / 2.0f, *name_tex, rgba(palette::text_dim));

        std::string value_label =
            std::to_string(static_cast<int>(core.celsius)) + "°C";
        const Texture *value_tex = cached_text(tcache, value_label, scale);
        if (value_tex)
            node_add_texture(root, cell_x + cell_w - kCpuCoreTextMargin - value_tex->width, cell_y + (kCpuCoreItemHeight - value_tex->height) / 2.0f, *value_tex, rgba(temp_color(core.celsius)));
    }

    return chrome.box_h;
}

float draw_gpu_temp_card(Node *root, TextureCache &tcache, int32_t scale, float x, float y, float w, const GpuTempState &gpu_temp, float origin_x, float origin_y) {
    x -= origin_x;
    y -= origin_y;

    std::string headline =
        std::to_string(static_cast<int>(gpu_temp.celsius)) + "°C";
    const Texture *headline_tex = cached_text_large(tcache, headline, scale);
    float content_h = measure_gpu_temp_content_h(tcache, scale, gpu_temp);

    PanelCardChrome chrome = panel_draw_card(root, tcache, scale, x, y, w, content_h, "GPU Temperature");
    float cx = chrome.content_x, cy = chrome.content_y;

    if (headline_tex)
        node_add_texture(root, cx, cy, *headline_tex, rgba(temp_color(gpu_temp.celsius)));

    return chrome.box_h;
}

} // namespace

void system_monitor_panel_paint(SystemMonitorPanelState &state, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, float pill_center_x, float bar_height, float bar_top_margin) {
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
    float panel_w = kPanelWidth;
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
    panel_draw_header(root, state.tcache, scale, "System", panel_x, panel_y, panel_w, state.click_regions);

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
            y += kPanelListSpacing;
        float row_h = row.height;
        bool row_visible = y + row_h > content_top && y < content_bottom;
        if (!row_visible) {
            y += row_h;
            continue;
        }

        switch (row.kind) {
        case RowKind::ResourcesCard:
            draw_resources_card(scroll_clip, state.tcache, scale, content_x, y, content_w, stats, gpu_temp, panel_x, content_top);
            break;
        case RowKind::CpuTempCard:
            draw_cpu_temp_card(scroll_clip, state.tcache, scale, content_x, y, content_w, cpu_temp, panel_x, content_top);
            break;
        case RowKind::GpuTempCard:
            draw_gpu_temp_card(scroll_clip, state.tcache, scale, content_x, y, content_w, gpu_temp, panel_x, content_top);
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
