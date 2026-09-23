#include <GLES3/gl32.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <string>

#include "modules/bar/panel/clock_panel.h"

#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

const char *month_name(int month) {
    static const std::array<const char *, 12> names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 0 || month > 11)
        return "";
    return names[static_cast<size_t>(month)];
}

const std::array<const char *, 7> kWeekdays = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};

const float kClockTodayText[4] = {1.0f, 1.0f, 1.0f, 1.0f};

float grid_content_width() { return kClockPanelWidth - 2.0f * kPanelPadding - kClockLeftColWidth - kClockColumnGap; }

float cell_size() { return grid_content_width() / 7.0f; }

float left_col_height() {
    return kClockWeekdayLineHeight + kClockLeftLineGap + kClockDateLineHeight + kClockLeftLineGap + kClockDateLineHeight + kClockBigDayGap + kClockBigDayRowHeight + kClockBigDayGap + kClockWeekLineHeight;
}

float grid_col_height() {
    return kClockGridHeaderHeight + kClockGridHeaderGap + kClockWeekdayRowHeight + kClockGridTopGap + 6.0f * cell_size();
}

float panel_height() {
    return kPanelPadding + std::max(left_col_height(), grid_col_height()) + kPanelPadding;
}

const Texture *cached_big_day(TextureCache &cache, const std::string &s, int32_t scale) {
    if (s.empty())
        return nullptr;
    return cache.get("big" + std::to_string(scale) + ":" + s, [&] { return rasterize_text_px(s, kClockBigDayFontPx, true, scale); });
}

int clock_panel_iso_week(int year, int month, int day) {
    std::chrono::sys_days sd{std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month + 1)} / std::chrono::day{static_cast<unsigned>(day)}};
    unsigned iso_wd = std::chrono::weekday{sd}.iso_encoding();
    std::chrono::sys_days thursday = sd + std::chrono::days{4 - static_cast<int>(iso_wd)};
    std::chrono::year thu_year = std::chrono::year_month_day{thursday}.year();
    std::chrono::sys_days jan1{thu_year / std::chrono::January / std::chrono::day{1}};
    return static_cast<int>((thursday - jan1).count() / 7) + 1;
}

void draw_nav_button(Node *root, TextureCache &cache, int32_t scale, std::vector<PanelClickRegion> &click_regions, float x, float y, const char *glyph, const std::string &tag) {
    using namespace panel_chrome_detail;
    Rect r = {x, y, kClockNavButtonSize, kClockNavButtonSize};
    node_add_rrect(root, r.x, r.y, r.w, r.h, kClockNavButtonSize / 2.0f, 0.0f, rgba(palette::overlay), rgba(palette::overlay));
    if (glyph) {
        const Texture *tex = cached_icon(cache, glyph, scale);
        if (tex)
            node_add_texture(root, r.x + (r.w - tex->width) / 2.0f, r.y + (r.h - tex->height) / 2.0f, *tex, rgba(palette::text));
    } else {
        float dot = 6.0f;
        node_add_rrect(root, r.x + (r.w - dot) / 2.0f, r.y + (r.h - dot) / 2.0f, dot, dot, dot / 2.0f, 0.0f, rgba(palette::accent), rgba(palette::accent));
    }
    click_regions.push_back({PanelClickKind::HeaderAction, r, tag});
}

} // namespace

bool clock_panel_create_surface(ClockPanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell, "astralia-shell-clock-panel", output);
}

bool clock_panel_init_egl(ClockPanelState &state, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state] {
        clock_panel_paint(state, state.pending_pill_center_x, state.pending_bar_height, state.pending_bar_top_margin);
    };
    return true;
}

void clock_panel_request_frame(ClockPanelState &state, float pill_center_x, float bar_height, float bar_top_margin) {
    state.pending_pill_center_x = pill_center_x;
    state.pending_bar_height = bar_height;
    state.pending_bar_top_margin = bar_top_margin;
    overlay_panel_request_frame(state.base);
}

void clock_panel_toggle(ClockPanelState &state, float pill_center_x) {
    panel_lock_toggle(state.base, state.locked_center_x, pill_center_x, [&state] { panel_reveal_open(state.reveal); }, [&state] {
            state.month_offset = 0;
            panel_reveal_close(state.reveal, state.base, [&state] { state.locked_center_x = -1.0f; }); });
}

void clock_panel_handle_click(ClockPanelState &state, double px, double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        if (region.kind == PanelClickKind::Close) {
            clock_panel_toggle(state);
        } else if (region.kind == PanelClickKind::HeaderAction) {
            if (region.tag == "prev")
                --state.month_offset;
            else if (region.tag == "next")
                ++state.month_offset;
            else
                state.month_offset = 0;
        }
        return;
    }

    if (!hit(state.panel_rect, px, py))
        clock_panel_toggle(state);
}

void clock_panel_handle_key_event(ClockPanelState &state, const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        clock_panel_toggle(state);
}

void clock_panel_paint(ClockPanelState &state, float pill_center_x, float bar_height, float bar_top_margin) {
    using namespace panel_chrome_detail;
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

    float panel_w = kClockPanelWidth;
    if (state.locked_center_x < 0.0f)
        state.locked_center_x = pill_center_x;
    float total_h = panel_height();
    float clip_h = panel_reveal_tick(state.reveal, state.base, total_h);
    float panel_h = std::max(0.0f, state.reveal.target);
    float panel_x = std::clamp(state.locked_center_x - panel_w / 2.0f, kPanelSideMargin, static_cast<float>(state.base.width) - panel_w - kPanelSideMargin);
    float panel_y = bar_height + bar_top_margin + kPanelGap;
    state.panel_rect = {panel_x, panel_y, panel_w, panel_h};

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    int today_y = local.tm_year + 1900;
    int today_m = local.tm_mon;
    int today_d = local.tm_mday;
    CalendarMonth disp =
        clock_panel_month_shifted(today_y, today_m, state.month_offset);

    panel_draw_box(root, panel_x, panel_y, panel_w, panel_h);

    float content_x = panel_x + kPanelPadding;
    float content_top = panel_y + kPanelPadding;
    float content_h = std::max(left_col_height(), grid_col_height());

    char weekday_buf[24];
    std::strftime(weekday_buf, sizeof(weekday_buf), "%A", &local);
    char month_buf[24];
    std::strftime(month_buf, sizeof(month_buf), "%B", &local);

    float ly = content_top + (content_h - left_col_height()) / 2.0f;
    auto centered_x = [&](const Texture *tex) { return content_x + (kClockLeftColWidth - tex->width) / 2.0f; };

    const Texture *weekday_tex = cached_text_large(state.tcache, weekday_buf, scale);
    if (weekday_tex)
        node_add_texture(root, centered_x(weekday_tex), ly + (kClockWeekdayLineHeight - weekday_tex->height) / 2.0f, *weekday_tex, rgba(palette::text));
    ly += kClockWeekdayLineHeight + kClockLeftLineGap;

    const Texture *month_tex = cached_text(state.tcache, month_buf, scale);
    if (month_tex)
        node_add_texture(root, centered_x(month_tex), ly + (kClockDateLineHeight - month_tex->height) / 2.0f, *month_tex, rgba(palette::text_muted));
    ly += kClockDateLineHeight + kClockLeftLineGap;

    const Texture *year_tex = cached_text(state.tcache, std::to_string(today_y), scale);
    if (year_tex)
        node_add_texture(root, centered_x(year_tex), ly + (kClockDateLineHeight - year_tex->height) / 2.0f, *year_tex, rgba(palette::text_muted));
    ly += kClockDateLineHeight + kClockBigDayGap;

    const Texture *big_day_tex = cached_big_day(state.tcache, std::to_string(today_d), scale);
    if (big_day_tex)
        node_add_texture(root, centered_x(big_day_tex), ly + (kClockBigDayRowHeight - big_day_tex->height) / 2.0f, *big_day_tex, rgba(palette::text));
    ly += kClockBigDayRowHeight + kClockBigDayGap;

    int week_no = clock_panel_iso_week(today_y, today_m, today_d);
    const Texture *week_tex = cached_text(state.tcache, "Week " + std::to_string(week_no), scale);
    if (week_tex)
        node_add_texture(root, centered_x(week_tex), ly + (kClockWeekLineHeight - week_tex->height) / 2.0f, *week_tex, rgba(palette::text_dim));

    float grid_x = content_x + kClockLeftColWidth + kClockColumnGap;
    float cell = cell_size();
    float grid_w = 7.0f * cell;

    const Texture *first_weekday_tex = cached_text(state.tcache, kWeekdays[0], scale);
    float grid_text_inset = first_weekday_tex ? (cell - first_weekday_tex->width) / 2.0f : 0.0f;

    std::string title =
        std::string(month_name(disp.month)) + " " + std::to_string(disp.year);
    const Texture *title_tex = cached_text(state.tcache, title, scale);
    if (title_tex)
        node_add_texture(root, grid_x + grid_text_inset, content_top + (kClockGridHeaderHeight - title_tex->height) / 2.0f, *title_tex, rgba(palette::text));

    float nav_y = content_top + (kClockGridHeaderHeight - kClockNavButtonSize) / 2.0f;
    float nav_x = grid_x + grid_w - kClockNavButtonSize;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x, nav_y, icon::chevron_right, "next");
    nav_x -= kClockNavButtonSize + kClockNavButtonGap;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x, nav_y, nullptr, "today");
    nav_x -= kClockNavButtonSize + kClockNavButtonGap;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x, nav_y, icon::chevron_left, "prev");

    float weekday_row_y = content_top + kClockGridHeaderHeight + kClockGridHeaderGap;
    for (int col = 0; col < 7; ++col) {
        const Texture *tex = cached_text(state.tcache, kWeekdays[static_cast<size_t>(col)], scale);
        if (!tex)
            continue;
        float cx = grid_x + col * cell + (cell - tex->width) / 2.0f;
        float cy = weekday_row_y + (kClockWeekdayRowHeight - tex->height) / 2.0f;
        node_add_texture(root, cx, cy, *tex, rgba(palette::text));
    }

    float grid_y = weekday_row_y + kClockWeekdayRowHeight + kClockGridTopGap;
    std::array<CalendarDay, 42> cells =
        clock_panel_cells(disp.year, disp.month);
    for (int i = 0; i < 42; ++i) {
        const CalendarDay &day = cells[static_cast<size_t>(i)];
        float cx = grid_x + (i % 7) * cell;
        float cy = grid_y + (i / 7) * cell;
        bool is_today = clock_panel_same_day(day, today_y, today_m, today_d);

        if (is_today) {
            float d = cell - kClockCellCirclePadding * 2.0f;
            node_add_rrect(root, cx + (cell - d) / 2.0f, cy + (cell - d) / 2.0f, d, d, d / 2.0f, 0.0f, rgba(palette::accent), rgba(palette::accent));
        }

        const Texture *tex =
            cached_text(state.tcache, std::to_string(day.day), scale);
        if (!tex)
            continue;
        const float *text_col = is_today       ? kClockTodayText
                                : day.in_month ? rgba(palette::text)
                                               : rgba(palette::text_dim);
        node_add_texture(root, cx + (cell - tex->width) / 2.0f, cy + (cell - tex->height) / 2.0f, *tex, text_col);
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

std::array<CalendarDay, 42> clock_panel_cells(int year, int month) {
    std::chrono::year_month_day first{
        std::chrono::year{year} /
        std::chrono::month{static_cast<unsigned>(month + 1)} /
        std::chrono::day{1}};
    std::chrono::sys_days first_days{first};
    unsigned start_offset =
        (std::chrono::weekday{first_days}.c_encoding() + 6u) % 7u;
    std::chrono::sys_days grid_start =
        first_days - std::chrono::days{static_cast<int>(start_offset)};

    std::array<CalendarDay, 42> cells{};
    for (int i = 0; i < 42; ++i) {
        std::chrono::year_month_day ymd{grid_start + std::chrono::days{i}};
        int cell_month =
            static_cast<int>(static_cast<unsigned>(ymd.month())) - 1;
        cells[static_cast<size_t>(i)] = {
            static_cast<int>(ymd.year()), cell_month,
            static_cast<int>(static_cast<unsigned>(ymd.day())),
            cell_month == month};
    }
    return cells;
}

CalendarMonth clock_panel_month_shifted(int year, int month, int delta) {
    std::chrono::year_month base =
        std::chrono::year{year} /
        std::chrono::month{static_cast<unsigned>(month + 1)};
    std::chrono::year_month shifted = base + std::chrono::months{delta};
    return {static_cast<int>(shifted.year()),
            static_cast<int>(static_cast<unsigned>(shifted.month())) - 1};
}

bool clock_panel_same_day(const CalendarDay &cell, int year, int month, int day) {
    return cell.year == year && cell.month == month && cell.day == day;
}
