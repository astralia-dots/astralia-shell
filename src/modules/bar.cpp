#include <GLES3/gl32.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "app/wayland_registry.h"

#include "core/log.h"

#include "modules/bar.h"
#include "modules/bar/styles/okinami.h"
#include "modules/bar/widget/clock_widget.h"
#include "modules/bar/widget/control_center_widget.h"
#include "modules/bar/widget/dock_widget.h"
#include "modules/bar/widget/logout_widget.h"
#include "modules/bar/widget/resource_widget.h"
#include "modules/bar/widget/status_widget.h"

#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/palette.h"

#include "service/dock_service.h"
#include "service/mpris_service.h"
#include "service/output_service.h"

BarPerMonitorState &bar_state(MonitorOutput &mon) {
    return mon.module<BarPerMonitorModule>()->state;
}

namespace bar_detail {

void bar_autohide_set_surface_geometry(zwlr_layer_surface_v1 *layer_surface, wl_surface *surface, wl_egl_window *egl_window, int32_t width, int32_t height_px, int32_t margin_top, int32_t margin_right, int32_t margin_left, int32_t exclusive_zone, int32_t output_scale) {
    zwlr_layer_surface_v1_set_size(layer_surface, 0, height_px);
    zwlr_layer_surface_v1_set_margin(layer_surface, margin_top, margin_right, 0, margin_left);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, exclusive_zone);
    wl_surface_commit(surface);
    if (egl_window)
        wl_egl_window_resize(egl_window, width * output_scale, height_px * output_scale, 0, 0);
}

void close_other_overlays(MonitorOutput &mon, PillId keep) {
    BarPerMonitorState &bs = bar_state(mon);
    if (keep != PillId::Logout) {
        if (Module *m = find_overlay_by_name(*mon.app, "logout"); m && m->is_open())
            m->toggle_from_widget(*mon.app);
    }
    if (keep != PillId::Wifi && bs.network_panel.base.open)
        network_panel_toggle(bs.network_panel);
    if (keep != PillId::Bluetooth && bs.bluetooth_panel.base.open)
        bluetooth_panel_toggle(bs.bluetooth_panel, mon.app->bluetooth);
    if (keep != PillId::Volume && bs.volume_panel.base.open)
        volume_panel_toggle(bs.volume_panel);
    if (keep != PillId::Tray && bs.tray_menu.base.popup)
        tray_menu_close(bs.tray_menu);
    if (keep != PillId::Tray && bs.tray_panel.base.open)
        tray_panel_toggle(bs.tray_panel);
    if (keep != PillId::Battery && bs.battery_panel.base.open)
        battery_panel_toggle(bs.battery_panel);
    if (keep != PillId::Cpu && bs.resource_panel.base.open)
        resource_panel_toggle(bs.resource_panel);
    if (keep != PillId::ControlCenter && bs.control_center_panel.base.open)
        control_center_panel_toggle(bs.control_center_panel);
    if (bs.clock_panel.base.open)
        clock_panel_toggle(bs.clock_panel);
}

int32_t bar_current_height(const MonitorOutput &mon) {
    return bar_autohide_geometry(mon.autohide.enabled, mon.autohide.collapsed, kBarHeight, bar_style_of(mon).top_margin, bar_hug_radius_px(mon))
        .height;
}

void bar_autohide_apply_geometry(MonitorOutput &mon, bool autohide, bool collapsed, const BarStyleSpec &style) {
    BarGeometry g =
        bar_autohide_geometry(autohide, collapsed, kBarHeight, style.top_margin, bar_hug_radius_px(mon));
    bar_autohide_set_surface_geometry(mon.layer_surface, mon.surface, mon.egl_window, mon.width, g.height, g.margin_top, style.side_margin, style.side_margin, g.exclusive_zone, mon.output_scale.scale);
}

void monitor_autohide_apply(MonitorOutput &mon, bool enabled, const BarStyleSpec &style) {
    mon.autohide.enabled = enabled;
    mon.autohide.hidden = false;
    mon.autohide.collapsed = enabled && mon.autohide.collapsed;
    mon.autohide.opacity = mon.autohide.collapsed ? 0.0f : 1.0f;
    bar_autohide_apply_geometry(mon, enabled, mon.autohide.collapsed, style);
}

} // namespace bar_detail

namespace {

void bar_dispatch_request_frame(WaylandState &app) {
    for (auto &mon : app.outputs)
        mon->module<BarPerMonitorModule>()->request_frame();
    app_detail::rest_egl_current(app);
}

void network_panel_dispatch(WaylandState &app, bool changed) {
    if (changed)
        bar_dispatch_request_frame(app);
}

void bluetooth_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

void volume_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

void tray_dispatch(WaylandState &app) { bar_dispatch_request_frame(app); }

void battery_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

void resource_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

void clock_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

void control_center_panel_dispatch(WaylandState &app) {
    bar_dispatch_request_frame(app);
}

} // namespace

bool bar_init_egl(MonitorOutput &mon, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context) {
    int32_t scale = mon.output_scale.scale;
    mon.egl_window =
        wl_egl_window_create(mon.surface, mon.width * scale, bar_detail::bar_current_height(mon) * scale);
    mon.egl_surface = eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(mon.egl_window), nullptr);
    if (mon.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, mon.egl_surface, context))
        return false;

    const char *renderer_name =
        reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    klog("egl: renderer=%s output='%s'", renderer_name ? renderer_name : "(unknown)", mon.output.name.c_str());

    mon.frame_clock.surface = mon.surface;
    mon.frame_clock.draw = [&mon] { bar_paint(mon); };
    (void)renderer;
    return true;
}

void bar_workspace_activate(MonitorOutput &mon, int ws_id) {
    compositor_focus_workspace(mon.app->compositor_state, ws_id, true);
}

void dispatch_pill_click(MonitorOutput &mon, double click_x, double click_y) {
    PointerState p = mon.app->pointer;
    p.x = click_x;
    p.y = click_y;
    if (mon.autohide.enabled)
        p.y -= bar_style_of(mon).top_margin;

    BarPerMonitorState &bs = bar_state(mon);
    if (bar_detail::workspace_row_hit_overview(bs.workspace_widget, p.x, p.y)) {
        if (Module *m = find_overlay_by_name(*mon.app, "overview"))
            m->toggle_from_widget(*mon.app);
        return;
    }
    int ws = bar_detail::workspace_row_hit_workspace(bs.workspace_widget, p.x, p.y);
    if (ws > 0) {
        bar_workspace_activate(mon, ws);
        return;
    }

    const Rect &cr = bs.clock_rect;
    if (cr.w > 0 && p.x >= cr.x && p.x < cr.x + cr.w && p.y >= cr.y && p.y < cr.y + cr.h) {
        bar_detail::clock_pill_clicked(mon);
        return;
    }

    bar_detail::dispatch_pill_click(bs.capsule, p, mon.surface);
}

void update_clock(MonitorOutput &mon) {
    bar_detail::update_clock(bar_state(mon).clock_texture);
}

void init_stub_widgets(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    bs.logout_texture = make_icon_texture(icon::power);
    bs.cpu_texture = make_icon_texture(icon::cpu);
    bs.control_center_texture = make_icon_texture(icon::dashboard);
    bs.overview_texture = make_icon_texture(icon::overview);
}

namespace {

void ensure_fillets(BarPerMonitorState &bs, const BarStyleSpec &style, int32_t scale) {
    int px = static_cast<int>(std::lround(style.fillet_radius * static_cast<float>(scale)));
    int inner_px = static_cast<int>(std::lround((style.fillet_radius + style.border_width) * static_cast<float>(scale)));
    if (px == bs.fillet_px && inner_px == bs.fillet_inner_px)
        return;
    auto build = [scale](Texture &tex, int size, bool circle_on_right) {
        std::vector<uint8_t> mask = fillet_rgba(size, circle_on_right);
        tex = make_texture_rgba(size, size, mask.data());
        tex.scale = scale;
    };
    build(bs.fillet_left, px, false);
    build(bs.fillet_right, px, true);
    build(bs.fillet_inner_left, inner_px, false);
    build(bs.fillet_inner_right, inner_px, true);
    bs.fillet_px = px;
    bs.fillet_inner_px = inner_px;
}

void ensure_hug_corner_textures(BarPerMonitorState &bs, const BarStyleSpec &style, int32_t hug_radius, int32_t scale) {
    int px = static_cast<int>(std::lround(static_cast<float>(hug_radius) * static_cast<float>(scale)));
    int inner_px = static_cast<int>(std::lround((static_cast<float>(hug_radius) + style.border_width) * static_cast<float>(scale)));
    if (px == bs.hug_px && inner_px == bs.hug_inner_px)
        return;
    if (px > 0) {
        auto build = [scale](Texture &tex, int size, bool circle_on_right) {
            std::vector<uint8_t> mask = fillet_rgba(size, circle_on_right);
            tex = make_texture_rgba(size, size, mask.data());
            tex.scale = scale;
        };
        build(bs.hug_outer_left, px, true);
        build(bs.hug_outer_right, px, false);
        build(bs.hug_inner_left, inner_px, true);
        build(bs.hug_inner_right, inner_px, false);
    }
    bs.hug_px = px;
    bs.hug_inner_px = inner_px;
}

} // namespace

void bar_paint(MonitorOutput &mon) {
    using namespace bar_detail;
    WaylandState &app = *mon.app;
    BarPerMonitorState &bs = bar_state(mon);
    const BarStyleSpec &style = bar_style_of(mon);
    const bool rail = bar_style_has_rail(style);
    bs.capsule.side_margin = static_cast<float>(style.side_margin);

    mon.animations.tick(std::chrono::steady_clock::now());

    Module *logout_m = find_overlay_by_name(app, "logout");
    Module *overview_m = find_overlay_by_name(app, "overview");
    bool overview_here = overview_m && overview_m->is_open() && overview_m->opened_by_widget() && overview_m->bound_output() == mon.output.wl;
    bool logout_here = logout_m && logout_m->is_open() && logout_m->opened_by_widget() && logout_m->bound_output() == mon.output.wl;
    PillId current_panel_pill = panel_pill(bs.network_panel, bs.bluetooth_panel, bs.volume_panel, bs.tray_panel, bs.battery_panel, bs.resource_panel, bs.control_center_panel, logout_here);

    if (mon.autohide.enabled) {
        bool want_shown = app.pointer.focused_surface == mon.surface || current_panel_pill != PillId::None || bs.clock_panel.base.open || overview_here;
        if (want_shown == mon.autohide.hidden) {
            mon.autohide.hidden = !want_shown;
            if (want_shown && mon.autohide.collapsed) {
                mon.autohide.collapsed = false;
                bar_autohide_apply_geometry(mon, true, false, style);
            }
            float target = want_shown ? 1.0f : 0.0f;
            float duration = want_shown ? kAutoHideRevealMs : kAutoHideHideMs;
            mon.animations.animate(mon.autohide.opacity, target, duration, Easing::EaseOutCubic, [&mon](float v) { mon.autohide.opacity = v; }, [&mon] {
                    if (mon.autohide.hidden && !mon.autohide.collapsed) {
                        mon.autohide.collapsed = true;
                        bar_autohide_apply_geometry(mon, true, true, bar_style_of(mon));
                    } }, kAutoHideAnimOwner);
        }
    }
    int32_t surface_height = bar_current_height(mon);
    float content_y_offset =
        mon.autohide.enabled ? static_cast<float>(style.top_margin) : 0.0f;
    float height = static_cast<float>(kBarHeight);

    gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    app.renderer.begin_frame(mon.width, surface_height, mon.output_scale.scale);
    app.renderer.set_opacity(mon.autohide.enabled ? mon.autohide.opacity : 1.0f);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    mon.scene.rebuild();
    Node *root = &mon.scene.root;
    Node *content = node_add_group(root, 0.0f, content_y_offset, static_cast<float>(mon.width), height);

    const float *white = rgba(palette::text);

    if (current_panel_pill == PillId::None && bs.capsule.panel_pill_prev != PillId::None) {
        bs.capsule.label_linger_pill = bs.capsule.panel_pill_prev;
        bs.capsule.label_linger_until =
            std::chrono::steady_clock::now() + kPillCloseLingerMs;
    }
    bs.capsule.panel_pill_prev = current_panel_pill;
    bool lingering =
        bs.capsule.label_linger_pill != PillId::None && std::chrono::steady_clock::now() < bs.capsule.label_linger_until;
    PointerState hit_pointer = app.pointer;
    hit_pointer.y -= content_y_offset;
    PillId hovered = current_panel_pill != PillId::None ? current_panel_pill : lingering ? bs.capsule.label_linger_pill
                                                                                         : hit_test_pills(bs.capsule, hit_pointer, mon.surface);
    if (hovered == PillId::None && bs.status_widget.volume_peek_active)
        hovered = PillId::Volume;

    float width = static_cast<float>(mon.width);
    float island_pad = rail ? kIslandPad : 0.0f;
    struct Island {
        Node *outer = nullptr;
        Node *inner = nullptr;
    };
    Island left_island;
    Island center_island;
    Island right_island;
    const float bw = style.border_width;
    const float radius = style.island_radius;
    if (rail) {
        ensure_fillets(bs, style, mon.output_scale.scale);
        node_add_rect(content, 0.0f, 0.0f, width, style.rail_height, rgba(style.border));
        auto add_outer = [&] { return node_add_rrect(content, 0.0f, -radius, 0.0f, height + radius, radius, 0.0f, rgba(style.border), rgba(style.border)); };
        left_island.outer = add_outer();
        center_island.outer = add_outer();
        right_island.outer = add_outer();
        node_add_rect(content, 0.0f, 0.0f, width, style.rail_height - bw, rgba(style.bg));
        auto add_inner = [&] { return node_add_rrect(content, 0.0f, -radius, 0.0f, height + radius - bw, radius - bw, 0.0f, rgba(style.bg), rgba(style.bg)); };
        left_island.inner = add_inner();
        center_island.inner = add_inner();
        right_island.inner = add_inner();
    }
    auto place_island = [&](Island &island, float left, float right, bool flush_left, bool flush_right) {
        island.outer->x = flush_left ? left - radius : left;
        island.outer->w = (flush_right ? right + radius : right) - island.outer->x;
        island.inner->x = flush_left ? island.outer->x : island.outer->x + bw;
        island.inner->w = island.outer->x + island.outer->w - (flush_right ? 0.0f : bw) - island.inner->x;
    };
    std::vector<std::pair<float, bool>> fillets;
    auto add_divider = [&](float cx) {
        node_add_rect(content, std::floor(cx), height * (1.0f - kIslandDividerHeightRatio) / 2.0f, 1.0f, height * kIslandDividerHeightRatio, rgba(palette::text_alpha20));
    };

    float x = island_pad;
    float item_from = x;
    std::vector<float> left_dividers;
    auto track_divider = [&] {
        if (rail && x > item_from)
            left_dividers.push_back(x - kCapsuleGap / 2.0f);
        item_from = x;
    };

    std::vector<Pill> logout_pills = {logout_pill(mon)};
    x = draw_pills(content, bs.capsule, mon.animations, x, height, logout_pills, white, style, hovered, current_panel_pill);
    track_divider();

    int active_id = app_detail::monitor_active_workspace_id(mon);
    const std::vector<Workspace> &ws_list = app_detail::monitor_workspaces(mon);
    x = draw_workspace_row(content, bs.workspace_widget, mon.animations, x, height, ws_list, active_id, style, bs.overview_texture);
    track_divider();

    std::vector<DockEntry> dock_entries =
        dock_entries_for_monitor(app.compositor_state, mon.output.name);
    x = draw_dock_capsule(content, bs.dock_widget, mon.animations, x, height, dock_entries, style);
    track_divider();

    bool left_flush = false;
    if (rail && x > island_pad) {
        float left_end = std::round(x - kCapsuleGap + island_pad);
        place_island(left_island, 0.0f, left_end, true, false);
        for (size_t i = 0; i + 1 < left_dividers.size(); ++i)
            add_divider(left_dividers[i]);
        fillets.emplace_back(left_end, true);
        left_flush = true;
    }

    bs.clock_rect = draw_clock_pill(content, height, mon.width, bs.clock_texture, white, style);
    if (rail && bs.clock_rect.w > 0.0f) {
        float center_left = std::round(bs.clock_rect.x - island_pad);
        float center_right = std::round(bs.clock_rect.x + bs.clock_rect.w + island_pad);
        place_island(center_island, center_left, center_right, false, false);
        fillets.emplace_back(center_left, false);
        fillets.emplace_back(center_right, true);
    }

    std::vector<Pill> control_center_pills = {control_center_pill(mon)};
    std::vector<Pill> status_segments = status_pills(mon);
    std::vector<Pill> right_stub_pills = {cpu_pill(mon)};

    float cc_w = pills_row_width(bs.capsule, mon.animations, control_center_pills, hovered, height, style);
    float status_w = pill_group_width(bs.capsule, mon.animations, status_segments, hovered, height, style, current_panel_pill);
    float stub_w = pills_row_width(bs.capsule, mon.animations, right_stub_pills, hovered, height, style, current_panel_pill);

    float cc_x = width - island_pad - cc_w;
    float status_x = cc_x - (status_w > 0 ? kCapsuleGap : 0.0f) - status_w;
    float stub_x = status_x - (stub_w > 0 ? kCapsuleGap : 0.0f) - stub_w;

    if (stub_w > 0) {
        draw_pills(content, bs.capsule, mon.animations, stub_x, height, right_stub_pills, white, style, hovered, current_panel_pill);
    }
    if (status_w > 0) {
        draw_pill_group(content, bs.capsule, mon.animations, status_x, height, status_segments, white, style, hovered, current_panel_pill);
        const UpowerState &u = app.upower;
        if (u.present && !u.charging && !u.full && u.percent <= 10) {
            const Rect &r = bs.capsule.pill_rects[pill_idx(PillId::Battery)];
            node_add_rrect(content, r.x, r.y, r.w, r.h, metrics::radius_md, 0.0f, rgba(palette::critical_alpha15), rgba(palette::critical_alpha15));
        }
    }
    if (cc_w > 0) {
        draw_pills(content, bs.capsule, mon.animations, cc_x, height, control_center_pills, white, style, hovered);
    }
    bool right_flush = false;
    if (rail && cc_w + status_w + stub_w > 0) {
        float leftmost = stub_w > 0 ? stub_x : status_w > 0 ? status_x
                                                            : cc_x;
        float right_left = std::round(leftmost - island_pad);
        place_island(right_island, right_left, width, false, true);
        if (stub_w > 0 && status_w + cc_w > 0)
            add_divider(stub_x + stub_w + kCapsuleGap / 2.0f);
        if (status_w > 0 && cc_w > 0)
            add_divider(status_x + status_w + kCapsuleGap / 2.0f);
        fillets.emplace_back(right_left, false);
        right_flush = true;
    }
    for (const auto &[edge_x, right_of_island] : fillets)
        node_add_texture(content, right_of_island ? edge_x : edge_x - style.fillet_radius, style.rail_height, right_of_island ? bs.fillet_right : bs.fillet_left, rgba(style.border));
    for (const auto &[edge_x, right_of_island] : fillets)
        node_add_texture(content, right_of_island ? edge_x - bw : edge_x - style.fillet_radius, style.rail_height - bw, right_of_island ? bs.fillet_inner_right : bs.fillet_inner_left, rgba(style.bg));

    int32_t hug_radius = bar_hug_radius_px(mon);
    if (hug_radius > 0) {
        ensure_hug_corner_textures(bs, style, hug_radius, mon.output_scale.scale);
        float hf = static_cast<float>(hug_radius);
        if (left_flush) {
            node_add_texture(content, 0.0f, height, bs.hug_outer_left, rgba(style.border));
            node_add_texture(content, -bw, height - bw, bs.hug_inner_left, rgba(style.bg));
        }
        if (right_flush) {
            node_add_texture(content, width - hf, height, bs.hug_outer_right, rgba(style.border));
            node_add_texture(content, width - hf, height - bw, bs.hug_inner_right, rgba(style.bg));
        }
    }

    mon.scene.draw(app.renderer);
    eglSwapBuffers(app.egl_display, mon.egl_surface);

    if (mon.animations.hasActive())
        bar_request_frame(mon);
}

void bar_request_frame(MonitorOutput &mon) {
    if (mon.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(mon.frame_clock);
}

bool BarPerMonitorModule::create_surface(WaylandState &app, MonitorOutput &mon, wl_output *output) {
    mon_ = &mon;
    mon.autohide.enabled = autohide_effective_enabled(app.cfg, mon.output.name);
    const BarStyleSpec &style = bar_style_spec(app.cfg.bar_style);
    LayerSurfaceConfig bar_cfg{
        .layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        .name_space = "astralia-shell",
        .anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT,
        .height = bar_detail::bar_current_height(mon),
        .margin_top = bar_detail::bar_autohide_geometry(mon.autohide.enabled, mon.autohide.collapsed, bar_detail::kBarHeight, style.top_margin, bar_hug_radius_px(mon)).margin_top,
        .margin_right = style.side_margin,
        .margin_left = style.side_margin,
        .exclusive_zone = bar_detail::bar_autohide_geometry(mon.autohide.enabled, mon.autohide.collapsed, bar_detail::kBarHeight, style.top_margin, bar_hug_radius_px(mon)).exclusive_zone,
    };
    mon.layer_surface = layer_surface_create(mon.surface, app.compositor, app.layer_shell, bar_cfg, &bar_layer_surface_listener, &mon, output);
    mon.output_scale.on_change = [&mon](int32_t scale) {
        if (mon.egl_window)
            wl_egl_window_resize(mon.egl_window, mon.width * scale, bar_detail::bar_current_height(mon) * scale, 0, 0);
        if (mon.frame_clock.surface)
            ::request_frame(mon.frame_clock);
    };
    output_scale_watch(mon.output_scale, mon.surface);
    wl_surface_commit(mon.surface);

    if (!network_panel_create_surface(state.network_panel, app.compositor, app.layer_shell, output))
        klog("network_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!bluetooth_panel_create_surface(state.bluetooth_panel, app.compositor, app.layer_shell, output))
        klog("bluetooth_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!volume_panel_create_surface(state.volume_panel, app.compositor, app.layer_shell, output))
        klog("volume_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!tray_panel_create_surface(state.tray_panel, app.compositor, app.layer_shell, output))
        klog("tray_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!battery_panel_create_surface(state.battery_panel, app.compositor, app.layer_shell, output))
        klog("battery_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!resource_panel_create_surface(state.resource_panel, app.compositor, app.layer_shell, output))
        klog("resource_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!clock_panel_create_surface(state.clock_panel, app.compositor, app.layer_shell, output))
        klog("clock_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    if (!control_center_panel_create_surface(state.control_center_panel, app.compositor, app.layer_shell, output))
        klog("control_center_panel: failed to create layer surface on '%s'", mon.output.name.c_str());
    return true;
}

bool BarPerMonitorModule::configured() const {
    return mon_->configured && (!state.network_panel.base.layer_surface || state.network_panel.base.configured) && (!state.bluetooth_panel.base.layer_surface || state.bluetooth_panel.base.configured) && (!state.volume_panel.base.layer_surface || state.volume_panel.base.configured) && (!state.tray_panel.base.layer_surface || state.tray_panel.base.configured) && (!state.battery_panel.base.layer_surface || state.battery_panel.base.configured) && (!state.resource_panel.base.layer_surface || state.resource_panel.base.configured) && (!state.clock_panel.base.layer_surface || state.clock_panel.base.configured) && (!state.control_center_panel.base.layer_surface || state.control_center_panel.base.configured);
}

bool BarPerMonitorModule::init_egl(WaylandState &app, MonitorOutput &mon) {
    if (!bar_init_egl(mon, app.renderer, app.egl_display, app.egl_config, app.egl_context))
        return false;
    update_clock(mon);
    init_stub_widgets(mon);

    state.network_panel.sync_text_input_focus = [this, &app](bool focused) {
        if (focused)
            app.text_input.set_focused_client(state.network_panel.base.surface, this);
        else
            app.text_input.clear_focused_client(this);
    };

    if (state.network_panel.base.layer_surface && network_panel_init_egl(state.network_panel, app.renderer, app.network, app.egl_display, app.egl_config, app.egl_context)) {
        network_panel_request_frame(state.network_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.bluetooth_panel.base.layer_surface && bluetooth_panel_init_egl(state.bluetooth_panel, app.renderer, app.bluetooth, app.egl_display, app.egl_config, app.egl_context)) {
        bluetooth_panel_request_frame(state.bluetooth_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.volume_panel.base.layer_surface && volume_panel_init_egl(state.volume_panel, app.renderer, app.pipewire, app.egl_display, app.egl_config, app.egl_context)) {
        volume_panel_request_frame(state.volume_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.tray_panel.base.layer_surface && tray_panel_init_egl(state.tray_panel, app.renderer, app.tray, app.egl_display, app.egl_config, app.egl_context)) {
        tray_panel_request_frame(state.tray_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.battery_panel.base.layer_surface && battery_panel_init_egl(state.battery_panel, app.renderer, app.upower, app.egl_display, app.egl_config, app.egl_context)) {
        battery_panel_request_frame(state.battery_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.resource_panel.base.layer_surface && resource_panel_init_egl(state.resource_panel, app.renderer, app.cpu_temp, app.gpu_temp, app.system_stats, app.egl_display, app.egl_config, app.egl_context)) {
        resource_panel_request_frame(state.resource_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.clock_panel.base.layer_surface && clock_panel_init_egl(state.clock_panel, app.renderer, app.egl_display, app.egl_config, app.egl_context)) {
        clock_panel_request_frame(state.clock_panel, 0.0f, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }
    if (state.control_center_panel.base.layer_surface && control_center_panel_init_egl(state.control_center_panel, app.renderer, app, app.egl_display, app.egl_config, app.egl_context)) {
        control_center_panel_request_frame(state.control_center_panel, 0.0f, 0.0f);
        gl_make_current(app.egl_display, mon.egl_surface, app.egl_context);
    }

    if (mon.autohide.enabled) {
        mon.autohide.hidden = true;
        mon.autohide.collapsed = true;
        mon.autohide.opacity = 0.0f;
    }
    bar_request_frame(mon);
    return true;
}

TextInputState BarPerMonitorModule::text_input_state() const {
    return network_panel_text_input_state(state.network_panel);
}

void BarPerMonitorModule::text_input_apply_edit(const TextInputEdit &edit) {
    network_panel_text_input_apply_edit(state.network_panel, edit);
    request_frame();
}

void BarPerMonitorModule::text_input_reset_preedit() {
    state.network_panel.password_field.preedit.clear();
    request_frame();
}

void BarPerMonitorModule::text_input_deactivated(TextInputService &) {
    state.network_panel.password_field.preedit.clear();
}

void BarPerMonitorModule::destroy(WaylandState &app, MonitorOutput &mon) {
    if (state.network_panel.sync_text_input_focus)
        state.network_panel.sync_text_input_focus(false);
    EGLDisplay d = app.egl_display;
    destroy_layer_surface(d, mon.surface, mon.layer_surface, mon.egl_window, mon.egl_surface, &mon.frame_clock);
    overlay_panel_destroy_surface(state.network_panel.base);
    overlay_panel_destroy_surface(state.bluetooth_panel.base);
    overlay_panel_destroy_surface(state.volume_panel.base);
    overlay_panel_destroy_surface(state.tray_panel.base);
    popup_window_destroy(state.tray_menu.base);
    overlay_panel_destroy_surface(state.battery_panel.base);
    overlay_panel_destroy_surface(state.resource_panel.base);
    overlay_panel_destroy_surface(state.clock_panel.base);
    overlay_panel_destroy_surface(state.control_center_panel.base);
}

bool BarPerMonitorModule::owns_surface(wl_surface *surface) const {
    return surface == mon_->surface || surface == state.network_panel.base.surface || surface == state.bluetooth_panel.base.surface || surface == state.volume_panel.base.surface || surface == state.tray_panel.base.surface || surface == state.tray_menu.base.surface || surface == state.battery_panel.base.surface || surface == state.resource_panel.base.surface || surface == state.clock_panel.base.surface || surface == state.control_center_panel.base.surface;
}

void BarPerMonitorModule::request_frame() {
    if (!mon_)
        return;
    bar_request_frame(*mon_);
    int32_t top_margin = bar_style_of(*mon_).top_margin;
    network_panel_request_frame(state.network_panel, bar_detail::pill_center_x(state.capsule, PillId::Wifi), static_cast<float>(bar_detail::kBarHeight), top_margin);
    bluetooth_panel_request_frame(state.bluetooth_panel, bar_detail::pill_center_x(state.capsule, PillId::Bluetooth), static_cast<float>(bar_detail::kBarHeight), top_margin);
    volume_panel_request_frame(state.volume_panel, bar_detail::pill_center_x(state.capsule, PillId::Volume), static_cast<float>(bar_detail::kBarHeight), top_margin);
    tray_panel_request_frame(state.tray_panel, bar_detail::pill_center_x(state.capsule, PillId::Tray), static_cast<float>(bar_detail::kBarHeight), top_margin);
    popup_window_request_frame(state.tray_menu.base);
    battery_panel_request_frame(state.battery_panel, bar_detail::pill_center_x(state.capsule, PillId::Battery), static_cast<float>(bar_detail::kBarHeight), top_margin);
    resource_panel_request_frame(state.resource_panel, bar_detail::pill_center_x(state.capsule, PillId::Cpu), static_cast<float>(bar_detail::kBarHeight), top_margin);
    clock_panel_request_frame(state.clock_panel, static_cast<float>(mon_->width) / 2.0f + state.capsule.side_margin, static_cast<float>(bar_detail::kBarHeight), top_margin);
    control_center_panel_request_frame(state.control_center_panel, static_cast<float>(bar_detail::kBarHeight), top_margin);
}

void BarPerMonitorModule::tick(WaylandState &, MonitorOutput &mon) {
    bar_detail::volume_pill_peek_tick(mon);

    int32_t hug = bar_hug_radius_px(mon);
    if (hug != state.applied_hug_radius_px) {
        state.applied_hug_radius_px = hug;
        bar_detail::bar_autohide_apply_geometry(mon, mon.autohide.enabled, mon.autohide.collapsed, bar_style_of(mon));
        bar_request_frame(mon);
    }

    if (state.tray_menu.base.done) {
        tray_menu_close(state.tray_menu);
        bar_request_frame(mon);
    }
}

void BarPerMonitorModule::timer_tick(WaylandState &app, MonitorOutput &mon) {
    update_clock(mon);
    if (state.network_panel.base.open)
        text_field_idle_toggle(state.network_panel.password_field);
    bar_request_frame(mon);
    if (bar_detail::volume_pill_peek_expire(mon))
        bar_request_frame(mon);

    if (state.resource_panel.base.open) {
        ++poll_tick_;
        if (poll_tick_ % 2 == 0) {
            cpu_temp_poll(app.cpu_temp);
            system_stats_poll(app.system_stats);
        }
        if (poll_tick_ % 5 == 0)
            gpu_temp_poll(app.gpu_temp);
        resource_panel_dispatch(app);
    }

    if (state.control_center_panel.base.open) {
        mpris_poll_position(app.mpris);
        control_center_panel_dispatch(app);
    }
}

bool BarPerMonitorModule::is_open() const {
    return state.network_panel.base.open || state.bluetooth_panel.base.open || state.volume_panel.base.open || state.tray_panel.base.open || state.battery_panel.base.open || state.resource_panel.base.open || state.clock_panel.base.open || state.control_center_panel.base.open;
}

void BarPerMonitorModule::handle_click(WaylandState &app, MonitorOutput &mon, wl_surface *surface, int button, double x, double y, uint32_t serial) {
    if (button != BTN_LEFT && surface != state.tray_panel.base.surface)
        return;

    if (surface == state.tray_panel.base.surface) {
        TrayPanelClickResult r = tray_panel_handle_click(state.tray_panel, app.tray, state.tray_menu, x, y, button);
        if (r.open_menu_for) {
            TrayMenuOpenArgs args{
                .compositor = app.compositor,
                .wm_base = app.wm_base,
                .parent_layer = state.tray_panel.base.layer_surface,
                .display = app.display,
                .egl_display = app.egl_display,
                .egl_config = app.egl_config,
                .egl_context = app.egl_context,
                .renderer = &app.renderer,
                .seat = app.seat,
                .grab_serial = serial,
                .pointer = &app.pointer,
            };
            tray_menu_open(state.tray_menu, app.tray, *r.open_menu_for, r.anchor_cell, args);
        }
        tray_dispatch(app);
        app_detail::rest_egl_current(app);
    } else if (surface == state.tray_menu.base.surface) {
        tray_menu_handle_click(state.tray_menu, app.tray, x, y);
        popup_window_request_frame(state.tray_menu.base);
        app_detail::rest_egl_current(app);
    } else if (surface == state.network_panel.base.surface) {
        network_panel_handle_click(state.network_panel, app.network, x, y);
        network_panel_dispatch(app, true);
    } else if (surface == state.bluetooth_panel.base.surface) {
        bluetooth_panel_handle_click(state.bluetooth_panel, app.bluetooth, x, y);
        bluetooth_panel_dispatch(app);
    } else if (surface == state.volume_panel.base.surface) {
        volume_panel_handle_click(state.volume_panel, app.pipewire, x, y);
        volume_panel_dispatch(app);
    } else if (surface == state.battery_panel.base.surface) {
        battery_panel_handle_click(state.battery_panel, x, y);
        battery_panel_dispatch(app);
    } else if (surface == state.resource_panel.base.surface) {
        resource_panel_handle_click(state.resource_panel, x, y);
        resource_panel_dispatch(app);
    } else if (surface == state.clock_panel.base.surface) {
        clock_panel_handle_click(state.clock_panel, x, y);
        clock_panel_dispatch(app);
    } else if (surface == state.control_center_panel.base.surface) {
        control_center_panel_handle_click(state.control_center_panel, app, x, y);
        control_center_panel_dispatch(app);
    } else if (surface == mon.surface) {
        dispatch_pill_click(mon, x, y);
        network_panel_dispatch(app, true);
        bluetooth_panel_dispatch(app);
        volume_panel_dispatch(app);
        tray_dispatch(app);
        battery_panel_dispatch(app);
        resource_panel_dispatch(app);
        clock_panel_dispatch(app);
        control_center_panel_dispatch(app);
        for (auto &m : app.overlays) {
            m->request_frame();
            app_detail::rest_egl_current(app);
        }
    }
}

void BarPerMonitorModule::handle_scroll(WaylandState &app, MonitorOutput &mon, wl_surface *surface, double dy) {
    if (surface == state.network_panel.base.surface) {
        network_panel_handle_scroll(state.network_panel, app.network, dy);
        network_panel_dispatch(app, true);
    } else if (surface == state.bluetooth_panel.base.surface) {
        bluetooth_panel_handle_scroll(state.bluetooth_panel, app.bluetooth, dy);
        bluetooth_panel_dispatch(app);
    } else if (surface == state.volume_panel.base.surface) {
        volume_panel_handle_scroll(state.volume_panel, app.pipewire, dy);
        volume_panel_dispatch(app);
    } else if (surface == state.battery_panel.base.surface) {
        battery_panel_handle_scroll(state.battery_panel, app.upower, dy);
        battery_panel_dispatch(app);
    } else if (surface == state.resource_panel.base.surface) {
        resource_panel_handle_scroll(state.resource_panel, app.cpu_temp, app.gpu_temp, app.system_stats, dy);
        resource_panel_dispatch(app);
    } else if (surface == state.control_center_panel.base.surface) {
        control_center_panel_handle_scroll(state.control_center_panel, dy);
        control_center_panel_dispatch(app);
    } else if (surface == mon.surface && bar_detail::hit_test_pills(state.capsule, app.pointer, mon.surface) == PillId::Volume) {
        bar_detail::volume_pill_handle_wheel(mon, dy);
    }
}

void BarPerMonitorModule::handle_key_event(WaylandState &app, MonitorOutput &mon, const KeyEvent &event) {
    (void)mon;
    if (state.tray_menu.base.popup) {
        tray_menu_handle_key_event(state.tray_menu, event);
        popup_window_request_frame(state.tray_menu.base);
        app_detail::rest_egl_current(app);
    } else if (state.network_panel.base.open) {
        network_panel_handle_key_event(state.network_panel, app.network, event);
        network_panel_dispatch(app, true);
    } else if (state.bluetooth_panel.base.open) {
        bluetooth_panel_handle_key_event(state.bluetooth_panel, app.bluetooth, event);
        bluetooth_panel_dispatch(app);
    } else if (state.volume_panel.base.open) {
        volume_panel_handle_key_event(state.volume_panel, app.pipewire, event);
        volume_panel_dispatch(app);
    } else if (state.battery_panel.base.open) {
        battery_panel_handle_key_event(state.battery_panel, event);
        battery_panel_dispatch(app);
    } else if (state.resource_panel.base.open) {
        resource_panel_handle_key_event(state.resource_panel, event);
        resource_panel_dispatch(app);
    } else if (state.clock_panel.base.open) {
        clock_panel_handle_key_event(state.clock_panel, event);
        clock_panel_dispatch(app);
    } else if (state.control_center_panel.base.open) {
        control_center_panel_handle_key_event(state.control_center_panel, app, event);
        control_center_panel_dispatch(app);
    }
}

void BarPerMonitorModule::handle_pointer_move(WaylandState &app, MonitorOutput &mon, double x, double y) {
    (void)mon;
    pointer_x_ = x;
    pointer_y_ = y;
    if (state.volume_panel.dragging) {
        volume_panel_handle_pointer_move(state.volume_panel, app.pipewire, x);
        request_frame();
    }
    if (state.control_center_panel.dragging) {
        control_center_panel_handle_pointer_move(state.control_center_panel, app, x);
        request_frame();
    }
}

void BarPerMonitorModule::handle_pointer_release() {
    if (state.volume_panel.dragging) {
        state.volume_panel.dragging.reset();
        request_frame();
    }
    if (state.control_center_panel.dragging) {
        state.control_center_panel.dragging.reset();
        request_frame();
    }
}

bool BarPerMonitorModule::wants_pointing_hand_cursor() const {
    if (!mon_ || mon_->app->pointer.focused_surface != mon_->surface)
        return false;

    BarPerMonitorState &bs = bar_state(*mon_);
    if (panel_region_hit(bs.network_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.bluetooth_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.volume_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.tray_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.tray_menu.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.battery_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.resource_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.clock_panel.click_regions, pointer_x_, pointer_y_) || panel_region_hit(bs.control_center_panel.click_regions, pointer_x_, pointer_y_))
        return true;

    PointerState p = mon_->app->pointer;
    p.x = pointer_x_;
    p.y = pointer_y_;
    if (mon_->autohide.enabled)
        p.y -= bar_style_of(*mon_).top_margin;
    const Rect &cr = bs.clock_rect;
    bool clock_hit = cr.w > 0 && p.x >= cr.x && p.x < cr.x + cr.w && p.y >= cr.y && p.y < cr.y + cr.h;
    return clock_hit || bar_detail::workspace_row_hit_overview(bs.workspace_widget, p.x, p.y) || bar_detail::workspace_row_hit_workspace(bs.workspace_widget, p.x, p.y) > 0 || bar_detail::hit_test_pills(bs.capsule, p, mon_->surface) != PillId::None;
}
