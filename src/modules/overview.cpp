#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "core/log.h"

#include "modules/overview.h"

#include "render/color_ops.h"
#include "render/gl.h"
#include "render/icon.h"
#include "render/layer_surface.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

const HyprMonitor *find_monitor_by_name(const HyprlandState &hypr, const std::string &name) {
    for (const HyprMonitor &m : hypr.monitors)
        if (m.name == name)
            return &m;
    return nullptr;
}

const HyprMonitor *find_monitor_by_id(const HyprlandState &hypr, int id) {
    for (const HyprMonitor &m : hypr.monitors)
        if (m.id == id)
            return &m;
    return nullptr;
}

double logical_w(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    return (rotated ? m.height : m.width) / (m.scale > 0.0 ? m.scale : 1.0);
}

double logical_h(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    return (rotated ? m.width : m.height) / (m.scale > 0.0 ? m.scale : 1.0);
}

double source_work_area_w(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    return logical_w(m) - (rotated ? m.reserved[1] : m.reserved[0]) - (rotated ? m.reserved[3] : m.reserved[2]);
}

double source_work_area_h(const HyprMonitor &m) {
    bool rotated = (m.transform % 2) == 1;
    return logical_h(m) - (rotated ? m.reserved[0] : m.reserved[1]) - (rotated ? m.reserved[2] : m.reserved[3]);
}

int workspaces_shown() { return kOverviewRows * kOverviewColumns; }

int active_workspace_id(const HyprlandState &hypr, const std::string &monitor_name) {
    auto it = hypr.by_monitor.find(monitor_name);
    if (it == hypr.by_monitor.end() || it->second.active_id < 0)
        return 1;
    return it->second.active_id;
}

int workspace_id_at(int workspace_group, int row, int col) {
    return workspace_group * workspaces_shown() + row * kOverviewColumns + col + 1;
}

struct LayoutCell {
    int workspace_id = -1;
    Rect rect;
};

struct Layout {
    std::vector<Rect> panels;
    float scale = 0.0f;
    std::vector<LayoutCell> cells;
};

struct Block {
    int group = 0;
    float x = 0.0f, y = 0.0f;
    float cell_w = 0.0f, cell_h = 0.0f;
};

std::string bound_output_name(const OverviewState &state, const WaylandState &app) {
    for (auto &mon : app.outputs)
        if (mon->output.wl == state.bound_output)
            return mon->output.name;
    return {};
}

const HyprMonitor *bound_monitor(const OverviewState &state, const WaylandState &app) {
    return find_monitor_by_name(app.hypr, bound_output_name(state, app));
}

int page_group(const HyprlandState &hypr, const HyprMonitor &m) {
    return (active_workspace_id(hypr, m.name) - 1) / workspaces_shown();
}

Block make_block(const HyprMonitor &m, int group, float scale, float x, float y) {
    Block b;
    b.group = group;
    b.x = x;
    b.y = y;
    b.cell_w = std::round(static_cast<float>(std::max(1.0, source_work_area_w(m)) * scale));
    b.cell_h = std::round(static_cast<float>(std::max(1.0, source_work_area_h(m)) * scale));
    return b;
}

std::vector<float> separation_steps(const std::vector<HyprMonitor> &monitors, double HyprMonitor::*pos, double (*extent)(const HyprMonitor &)) {
    std::vector<size_t> order(monitors.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return monitors[a].*pos < monitors[b].*pos; });
    std::vector<float> steps(monitors.size(), 0.0f);
    for (size_t i = 1; i < order.size(); ++i)
        for (size_t j = 0; j < i; ++j) {
            const HyprMonitor &before = monitors[order[j]];
            const HyprMonitor &after = monitors[order[i]];
            if (before.*pos + extent(before) <= after.*pos + 0.5)
                steps[order[i]] = std::max(steps[order[i]], steps[order[j]] + 1.0f);
        }
    return steps;
}

float global_blocks(const HyprlandState &hypr, int surface_w, int surface_h, std::vector<Block> &blocks) {
    if (hypr.monitors.empty())
        return 0.0f;
    float spacing = std::round(kOverviewWorkspaceSpacing);
    float gap_x = spacing * (kOverviewColumns - 1) + kOverviewBackgroundPadding * 2.0f + kOverviewGlobalBlockSpacing;
    float gap_y = spacing * (kOverviewRows - 1) + kOverviewBackgroundPadding * 2.0f + kOverviewGlobalBlockSpacing;
    float frame = 2.0f * (kOverviewBackgroundPadding + kOverviewElevationMargin);

    const std::vector<HyprMonitor> &ms = hypr.monitors;
    std::vector<float> steps_x = separation_steps(ms, &HyprMonitor::x, logical_w);
    std::vector<float> steps_y = separation_steps(ms, &HyprMonitor::y, logical_h);
    double min_x = ms[0].x, min_y = ms[0].y;
    for (const HyprMonitor &m : ms) {
        min_x = std::min(min_x, m.x);
        min_y = std::min(min_y, m.y);
    }

    float scale = kOverviewGlobalScale;
    for (size_t i = 0; i < ms.size(); ++i) {
        float fixed_x = steps_x[i] * gap_x + spacing * (kOverviewColumns - 1) + frame;
        float fixed_y = steps_y[i] * gap_y + spacing * (kOverviewRows - 1) + frame;
        float per_x = static_cast<float>((ms[i].x - min_x + std::max(1.0, source_work_area_w(ms[i]))) * kOverviewColumns);
        float per_y = static_cast<float>((ms[i].y - min_y + std::max(1.0, source_work_area_h(ms[i]))) * kOverviewRows);
        scale = std::min({scale, (surface_w - fixed_x) / per_x, (surface_h - fixed_y) / per_y});
    }
    scale = std::max(scale, 0.01f);

    for (size_t i = 0; i < ms.size(); ++i) {
        float x = static_cast<float>((ms[i].x - min_x) * kOverviewColumns * scale) + steps_x[i] * gap_x;
        float y = static_cast<float>((ms[i].y - min_y) * kOverviewRows * scale) + steps_y[i] * gap_y;
        blocks.push_back(make_block(ms[i], page_group(hypr, ms[i]), scale, x, y));
    }
    return scale;
}

Layout compute_layout(const OverviewState &state, const WaylandState &app) {
    Layout g;
    std::vector<Block> blocks;
    if (state.global_mode) {
        g.scale = global_blocks(app.hypr, state.base.width, state.base.height, blocks);
    } else if (const HyprMonitor *target = bound_monitor(state, app)) {
        g.scale = kOverviewScale;
        blocks.push_back(make_block(*target, state.workspace_group, g.scale, 0.0f, 0.0f));
    }
    if (blocks.empty())
        return g;

    float spacing = std::round(kOverviewWorkspaceSpacing);
    float grid_w = 0.0f, grid_h = 0.0f;
    for (const Block &b : blocks) {
        grid_w = std::max(grid_w, b.x + b.cell_w * kOverviewColumns + spacing * (kOverviewColumns - 1));
        grid_h = std::max(grid_h, b.y + b.cell_h * kOverviewRows + spacing * (kOverviewRows - 1));
    }
    float pad = kOverviewBackgroundPadding;
    float root_w = grid_w + pad * 2.0f + kOverviewElevationMargin * 2.0f;
    float root_h = grid_h + pad * 2.0f + kOverviewElevationMargin * 2.0f;
    float origin_x = std::round((state.base.width - root_w) / 2.0f) + kOverviewElevationMargin;
    float origin_y = std::round((state.base.height - root_h) / 2.0f) + state.slide_y + kOverviewElevationMargin;

    for (const Block &b : blocks) {
        g.panels.push_back({origin_x + b.x, origin_y + b.y, b.cell_w * kOverviewColumns + spacing * (kOverviewColumns - 1) + pad * 2.0f, b.cell_h * kOverviewRows + spacing * (kOverviewRows - 1) + pad * 2.0f});
        for (int row = 0; row < kOverviewRows; ++row)
            for (int col = 0; col < kOverviewColumns; ++col)
                g.cells.push_back({workspace_id_at(b.group, row, col), {origin_x + b.x + pad + static_cast<float>(col) * (b.cell_w + spacing), origin_y + b.y + pad + static_cast<float>(row) * (b.cell_h + spacing), b.cell_w, b.cell_h}});
    }
    return g;
}

const LayoutCell *find_cell(const Layout &g, int workspace_id) {
    for (const LayoutCell &c : g.cells)
        if (c.workspace_id == workspace_id)
            return &c;
    return nullptr;
}

const LayoutCell *cell_at(const Layout &g, double px, double py) {
    for (const LayoutCell &c : g.cells)
        if (px >= c.rect.x && px < c.rect.x + c.rect.w && py >= c.rect.y && py < c.rect.y + c.rect.h)
            return &c;
    return nullptr;
}

uint64_t tile_anim_owner(const std::string &address, int component) {
    return 1000 + (std::hash<std::string>{}(address) << 2) + static_cast<uint64_t>(component);
}

void animate_tile_rect(OverviewState &state, const std::string &address, const Rect &from, const Rect &to) {
    auto anim = [&](int component, float from_v, float to_v, float Rect::*field) {
        state.base.animations.animate(from_v, to_v, kOverviewAnimFastMs, Easing::EaseOutCubic, [&state, address, field](float v) { state.tile_anim[address].current.*field = v; }, {}, tile_anim_owner(address, component));
    };
    anim(0, from.x, to.x, &Rect::x);
    anim(1, from.y, to.y, &Rect::y);
    anim(2, from.w, to.w, &Rect::w);
    anim(3, from.h, to.h, &Rect::h);
}

bool rect_equal(const Rect &a, const Rect &b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void rebuild_tiles(OverviewState &state, WaylandState &app, const Layout &g, const HyprMonitor *fallback) {
    state.tiles.clear();

    std::vector<const HyprClient *> visible;
    for (const HyprClient &c : app.hypr.clients)
        if (find_cell(g, c.workspace_id))
            visible.push_back(&c);

    std::sort(visible.begin(), visible.end(), [](const HyprClient *a, const HyprClient *b) {
        if (a->pinned != b->pinned)
            return !a->pinned;
        if (a->floating != b->floating)
            return !a->floating;
        if ((a->fullscreen > 0) != (b->fullscreen > 0))
            return !(a->fullscreen > 0);
        if (a->workspace_id != b->workspace_id)
            return a->workspace_id < b->workspace_id;
        return a->focus_history_id > b->focus_history_id;
    });

    for (const HyprClient *c : visible) {
        const HyprMonitor *src = find_monitor_by_id(app.hypr, c->monitor_id);
        if (!src)
            src = fallback;
        if (!src)
            continue;

        Rect cell = find_cell(g, c->workspace_id)->rect;

        double src_w = std::max(1.0, source_work_area_w(*src));
        double src_h = std::max(1.0, source_work_area_h(*src));
        double scale = std::min(cell.w / src_w, cell.h / src_h);

        double raw_x =
            std::max((c->at[0] - src->x - src->reserved[0]) * scale, 0.0);
        double raw_y =
            std::max((c->at[1] - src->y - src->reserved[1]) * scale, 0.0);
        double raw_w = std::max(1.0, c->size[0] * scale);
        double raw_h = std::max(1.0, c->size[1] * scale);

        double base_w = std::min(raw_w, static_cast<double>(cell.w));
        double base_h = std::min(raw_h, static_cast<double>(cell.h));
        double base_x = std::clamp(raw_x, 0.0, std::max(0.0, cell.w - base_w));
        double base_y = std::clamp(raw_y, 0.0, std::max(0.0, cell.h - base_h));

        OverviewWindowTile tile;
        tile.address = c->address;
        tile.workspace_id = c->workspace_id;
        tile.window_class = c->window_class;
        tile.rect = {static_cast<float>(cell.x + base_x), static_cast<float>(cell.y + base_y), static_cast<float>(base_w), static_cast<float>(base_h)};
        state.tiles.push_back(tile);

        OverviewTileAnim &anim = state.tile_anim[tile.address];
        if (!anim.seen) {
            anim.current = tile.rect;
            anim.target = tile.rect;
            anim.seen = true;
        } else if (!rect_equal(anim.target, tile.rect)) {
            animate_tile_rect(state, tile.address, anim.target, tile.rect);
            anim.target = tile.rect;
        }
    }

    for (auto it = state.tile_anim.begin(); it != state.tile_anim.end();) {
        bool live = std::any_of(state.tiles.begin(), state.tiles.end(), [&](const OverviewWindowTile &t) { return t.address == it->first; });
        it = live ? std::next(it) : state.tile_anim.erase(it);
    }
}

bool point_in_rect(double px, double py, const Rect &r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

} // namespace

bool overview_create_surface(OverviewState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell, "astralia-shell-overview", output);
}

bool overview_init_egl(OverviewState &state, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state] {
        overview_paint(state, *state.app_ptr);
    };
    return true;
}

void overview_retarget(OverviewState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_display *display, Renderer &renderer, EGLDisplay egl_display, EGLConfig egl_config, EGLContext egl_context, wl_output *target_output, const char *target_name) {
    wl_output *bound = overlay_panel_retarget(state.base, display, state.bound_output, target_output, target_name, [&](wl_output *out) { return overview_create_surface(state, compositor, layer_shell, out); }, [&] { return overview_init_egl(state, renderer, egl_display, egl_config, egl_context); });
    if (bound)
        state.bound_output = bound;
}

void overview_request_frame(OverviewState &state) {
    overlay_panel_request_frame(state.base);
}

void overview_toggle(OverviewState &state, WaylandState &app, bool by_widget) {
    if (app.compositor_backend != WaylandState::CompositorBackend::Hyprland)
        return;
    if (!state.base.layer_surface || state.base.egl_surface == EGL_NO_SURFACE)
        return;

    bool opening = !state.base.open;
    if (opening) {
        hypr_refresh(app.hypr);
        state.opened_by_widget = by_widget;
        int active_id = active_workspace_id(app.hypr, bound_output_name(state, app));
        state.selected_workspace = active_id;
        state.workspace_group = (active_id - 1) / workspaces_shown();
        state.slide_y = static_cast<float>(state.base.height);
    } else {
        state.dragging = false;
    }
    overlay_panel_toggle(state.base);
    state.base.animations.animate(state.slide_y, opening ? 0.0f : static_cast<float>(state.base.height), kOverviewAnimFastMs, Easing::EaseOutCubic, [&state](float v) { state.slide_y = v; }, {}, kOverviewSlideOwner);
    overview_request_frame(state);
}

std::vector<IpcHandler> overview_ipc_handlers(OverviewState &overview, WaylandState &state) {
    return {
        {"overview",
         [&overview, &state] {
             if (!overview.base.open) {
                 MonitorOutput *target = app_detail::active_target_monitor(state);
                 if (target && (target->output.wl != overview.bound_output || !overview.base.layer_surface))
                     overview_retarget(overview, state.compositor, state.layer_shell, state.display, state.renderer, state.egl_display, state.egl_config, state.egl_context, target->output.wl, target->output.name.c_str());
             }
             overview_toggle(overview, state);
         },
         "toggle the overview (Hyprland only)"},
    };
}

void overview_handle_click(OverviewState &state, WaylandState &app, double px, double py) {
    for (const OverviewWindowTile &tile : state.tiles) {
        auto it = state.tile_anim.find(tile.address);
        const Rect &drawn =
            it != state.tile_anim.end() ? it->second.current : tile.rect;
        if (!point_in_rect(px, py, drawn))
            continue;
        state.dragging = true;
        state.drag_address = tile.address;
        state.drag_from_workspace = tile.workspace_id;
        state.drag_target_workspace = -1;
        state.drag_offset_x = px - drawn.x;
        state.drag_offset_y = py - drawn.y;
        state.drag_pointer_x = px;
        state.drag_pointer_y = py;
        return;
    }

    Layout g = compute_layout(state, app);
    if (g.cells.empty()) {
        overview_toggle(state, app);
        return;
    }
    if (const LayoutCell *cell = cell_at(g, px, py)) {
        state.selected_workspace = cell->workspace_id;
        hypr_tile_focus_workspace(app.hypr, cell->workspace_id, state.global_mode);
        return;
    }

    float m = kOverviewElevationMargin;
    bool inside = std::any_of(g.panels.begin(), g.panels.end(), [&](const Rect &p) { return point_in_rect(px, py, {p.x - m, p.y - m, p.w + 2.0f * m, p.h + 2.0f * m}); });
    if (!inside)
        overview_toggle(state, app);
}

bool overview_point_is_clickable(OverviewState &state, WaylandState &app, double px, double py) {
    if (!state.base.open)
        return false;
    for (const OverviewWindowTile &tile : state.tiles) {
        auto it = state.tile_anim.find(tile.address);
        const Rect &drawn =
            it != state.tile_anim.end() ? it->second.current : tile.rect;
        if (point_in_rect(px, py, drawn))
            return true;
    }
    return cell_at(compute_layout(state, app), px, py) != nullptr;
}

void overview_handle_pointer_move(OverviewState &state, WaylandState &app, double px, double py) {
    if (!state.dragging)
        return;
    state.drag_pointer_x = px;
    state.drag_pointer_y = py;

    Layout g = compute_layout(state, app);
    const LayoutCell *cell = cell_at(g, px, py);
    state.drag_target_workspace = cell ? cell->workspace_id : -1;
}

void overview_handle_pointer_release(OverviewState &state, WaylandState &app) {
    if (!state.dragging)
        return;
    state.dragging = false;

    Layout g = compute_layout(state, app);
    const LayoutCell *cell = cell_at(g, state.drag_pointer_x, state.drag_pointer_y);
    if (!cell)
        return;
    if (cell->workspace_id != state.drag_from_workspace) {
        hypr_tile_move_window(app.hypr, cell->workspace_id, false, state.drag_address, state.global_mode);
    } else {
        hypr_tile_focus_workspace(app.hypr, cell->workspace_id, state.global_mode);
    }
}

void overview_handle_key_event(OverviewState &state, WaylandState &app, const KeyEvent &event) {
    if (!state.base.open)
        return;
    int shown = workspaces_shown();
    if (state.global_mode)
        state.workspace_group = (state.selected_workspace - 1) / shown;

    auto switch_to = [&](int ws, bool shift, bool alt) {
        state.selected_workspace = ws;
        state.workspace_group = (ws - 1) / shown;
        if (shift)
            hypr_tile_swap_workspace(app.hypr, ws, state.global_mode);
        else if (alt)
            hypr_tile_move_workspace_in(app.hypr, ws, state.global_mode);
        else
            hypr_tile_focus_workspace(app.hypr, ws, state.global_mode);
    };

    switch (event.kind) {
    case KeyKind::Left:
    case KeyKind::Right:
    case KeyKind::Up:
    case KeyKind::Down: {
        int current = (state.selected_workspace - 1) % shown;
        if (current < 0)
            current += shown;
        int col = current % kOverviewColumns;
        int row = current / kOverviewColumns;
        if (event.kind == KeyKind::Left)
            col = (col - 1 + kOverviewColumns) % kOverviewColumns;
        else if (event.kind == KeyKind::Right)
            col = (col + 1) % kOverviewColumns;
        else if (event.kind == KeyKind::Up)
            row = (row - 1 + kOverviewRows) % kOverviewRows;
        else
            row = (row + 1) % kOverviewRows;
        switch_to(workspace_id_at(state.workspace_group, row, col), event.shift, event.alt);
        break;
    }
    case KeyKind::Tab:
        state.global_mode = !state.global_mode;
        state.dragging = false;
        state.indicator_tracking = false;
        state.workspace_group = (state.selected_workspace - 1) / shown;
        break;
    case KeyKind::Escape:
        overview_toggle(state, app);
        break;
    case KeyKind::Text:
        if (event.text.size() == 1 && event.text[0] >= '0' && event.text[0] <= '9') {
            int position = event.text[0] == '0' ? 10 : event.text[0] - '0';
            if (position <= shown)
                switch_to(state.workspace_group * shown + position, event.shift, event.alt);
        } else if (event.ctrl && (event.text == "d" || event.text == "D")) {
            hypr_tile_close_workspace(app.hypr, HyprCloseScope::All);
        } else if (event.text == "D") {
            const HyprMonitor *target = bound_monitor(state, app);
            if (target)
                hypr_tile_close_workspace(app.hypr, HyprCloseScope::Monitor, target->id);
        } else if (event.text == "d") {
            hypr_tile_close_workspace(app.hypr, HyprCloseScope::Workspace, state.selected_workspace);
        }
        break;
    default:
        break;
    }
    overview_request_frame(state);
}

void overview_paint(OverviewState &state, WaylandState &app) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    gl_make_current(state.base.egl_display, state.base.egl_surface, state.base.egl_context);
    state.renderer->begin_frame(state.base.width, state.base.height, state.base.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    if (state.base.open && app.compositor_backend == WaylandState::CompositorBackend::Hyprland) {
        Layout g = compute_layout(state, app);

        if (!g.cells.empty()) {
            rebuild_tiles(state, app, g, bound_monitor(state, app));
            if (state.dragging)
                std::stable_partition(state.tiles.begin(), state.tiles.end(), [&](const OverviewWindowTile &t) { return t.address != state.drag_address; });

            static const float bg_fill[4] = {
                palette::field_bg.r, palette::field_bg.g, palette::field_bg.b,
                palette::field_bg.a * kOverviewBackgroundOpacity};
            static const float bg_border[4] = {
                palette::accent.r, palette::accent.g, palette::accent.b,
                palette::accent.a};
            for (const Rect &panel : g.panels) {
                Node *bg = state.scene.root.claim_child();
                bg->kind = NodeKind::RoundedRect;
                bg->x = panel.x;
                bg->y = panel.y;
                bg->w = panel.w;
                bg->h = panel.h;
                bg->radius = kOverviewScreenRounding * g.scale + kOverviewBackgroundPadding;
                bg->border_width = kOverviewBackgroundBorderWidth;
                bg->fill = bg_fill;
                bg->border = bg_border;
            }

            int active_id = active_workspace_id(app.hypr, state.global_mode ? app.hypr.focused_monitor : bound_output_name(state, app));
            int active_page = (active_id - 1) / workspaces_shown();
            const LayoutCell *active_cell = find_cell(g, active_id);
            if (!active_cell)
                state.indicator_tracking = false;

            for (const LayoutCell &layout_cell : g.cells) {
                const Rect &cell = layout_cell.rect;
                int ws = layout_cell.workspace_id;
                bool hovered_while_dragging =
                    state.dragging && state.drag_target_workspace == ws;

                Node *cellnode = state.scene.root.claim_child();
                cellnode->kind = NodeKind::RoundedRect;
                cellnode->x = cell.x;
                cellnode->y = cell.y;
                cellnode->w = cell.w;
                cellnode->h = cell.h;
                cellnode->radius = kOverviewScreenRounding * g.scale;
                cellnode->border_width = kOverviewWorkspaceBorderWidth;
                static const float cell_fill[4] = {
                    palette::field_bg.r, palette::field_bg.g,
                    palette::field_bg.b, palette::field_bg.a};
                static const float cell_border[4] = {
                    palette::text.r, palette::text.g, palette::text.b,
                    0.18f};
                static const float cell_border_hover[4] = {
                    palette::text.r, palette::text.g, palette::text.b,
                    0.08f};
                cellnode->fill = cell_fill;
                cellnode->border = hovered_while_dragging ? cell_border_hover : cell_border;

                Texture &num_tex = state.workspace_number_tex[ws];
                if (!num_tex.id) {
                    RasterizedText num = rasterize_text_large(std::to_string(ws), state.base.output_scale.scale);
                    if (num.width > 0)
                        num_tex = make_texture_from_raster(num);
                }
                if (num_tex.id && num_tex.width <= cell.w && num_tex.height <= cell.h) {
                    static const float num_tint[4] = {
                        palette::text.r, palette::text.g, palette::text.b,
                        1.0f - kOverviewWorkspaceNumberTextFade};
                    Node *label = state.scene.root.claim_child();
                    label->kind = NodeKind::Texture;
                    label->x = cell.x + (cell.w - num_tex.width) / 2.0f;
                    label->y = cell.y + (cell.h - num_tex.height) / 2.0f;
                    label->w = static_cast<float>(num_tex.width);
                    label->h = static_cast<float>(num_tex.height);
                    label->tex = &num_tex;
                    label->tint = num_tint;
                }
            }

            for (const OverviewWindowTile &tile : state.tiles) {
                toplevel_export_request(state.capture, app.toplevel_export_manager, app.shm, tile.address, kOverviewCaptureIntervalMs);
                const Texture *tex =
                    toplevel_export_texture(state.capture, tile.address);

                auto anim_it = state.tile_anim.find(tile.address);
                Rect r = anim_it != state.tile_anim.end() ? anim_it->second.current : tile.rect;
                if (state.dragging && state.drag_address == tile.address) {
                    r.x = static_cast<float>(state.drag_pointer_x - state.drag_offset_x);
                    r.y = static_cast<float>(state.drag_pointer_y - state.drag_offset_y);
                }

                if (tex && tex->id) {
                    Node *n = state.scene.root.claim_child();
                    n->kind = NodeKind::RoundedTexture;
                    n->x = r.x;
                    n->y = r.y;
                    n->w = r.w;
                    n->h = r.h;
                    n->radius = kOverviewWindowRounding * g.scale;
                    n->tex = tex;
                    static const float white[4] = {1, 1, 1, 1};
                    n->tint = white;
                } else {
                    Node *n = state.scene.root.claim_child();
                    n->kind = NodeKind::RoundedRect;
                    n->x = r.x;
                    n->y = r.y;
                    n->w = r.w;
                    n->h = r.h;
                    n->radius = kOverviewWindowRounding * g.scale;
                    n->border_width = kOverviewWindowPreviewBorderWidth;
                    static const float fill[4] = {
                        palette::field_bg.r, palette::field_bg.g,
                        palette::field_bg.b, palette::field_bg.a};
                    static const float border[4] = {
                        palette::accent.r, palette::accent.g, palette::accent.b,
                        palette::accent.a};
                    n->fill = fill;
                    n->border = border;
                }

                const Texture *icon = state.icons.lookup(tile.window_class);
                if (icon) {
                    static const float icon_tint[4] = {1, 1, 1, 1};
                    float size = std::round(std::min(r.w, r.h) * kOverviewIconToWindowRatio);
                    node_add_texture_rect(&state.scene.root, r.x + r.w - size - kOverviewIconInset, r.y + r.h - size - kOverviewIconInset, size, size, *icon, icon_tint);
                }
            }

            std::vector<std::string> live_addresses;
            for (const HyprClient &c : app.hypr.clients)
                live_addresses.push_back(c.address);
            toplevel_export_prune(state.capture, live_addresses);

            if (active_cell && state.slide_y == 0.0f) {
                Rect cell = active_cell->rect;
                if (!state.indicator_tracking || state.indicator_page != active_page) {
                    state.indicator_anim = cell;
                    state.indicator_target = cell;
                    state.indicator_tracking = true;
                    state.indicator_page = active_page;
                } else if (state.indicator_target.x != cell.x || state.indicator_target.y != cell.y) {
                    state.base.animations.animate(state.indicator_anim.x, cell.x, kOverviewAnimFastMs, Easing::EaseOutCubic, [&state](float v) { state.indicator_anim.x = v; }, {}, kOverviewIndicatorXOwner);
                    state.base.animations.animate(state.indicator_anim.y, cell.y, kOverviewAnimFastMs, Easing::EaseOutCubic, [&state](float v) { state.indicator_anim.y = v; }, {}, kOverviewIndicatorYOwner);
                    state.indicator_target = cell;
                }

                Node *indicator = state.scene.root.claim_child();
                indicator->kind = NodeKind::RoundedRect;
                indicator->x = state.indicator_anim.x;
                indicator->y = state.indicator_anim.y;
                indicator->w = cell.w;
                indicator->h = cell.h;
                indicator->radius = kOverviewScreenRounding * g.scale;
                indicator->border_width = kOverviewFocusedIndicatorBorderWidth;
                static const float transparent[4] = {0, 0, 0, 0};
                static const float indicator_border[4] = {
                    palette::accent_alt.r, palette::accent_alt.g,
                    palette::accent_alt.b, palette::accent_alt.a};
                indicator->fill = transparent;
                indicator->border = indicator_border;
            }
        }
    }

    state.renderer->set_opacity(state.base.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive() || state.dragging)
        overlay_panel_request_frame(state.base);
}
