#include <GLES2/gl2.h>
#include <algorithm>
#include <cstring>
#include <wayland-client.h>

#include "wayland/bootstrap.h"

#include "app/module_registry.h"
#include "app/monitor_output.h"
#include "app/wayland_registry.h"
#include "app/wayland_state.h"

#include "core/log.h"

#include "modules/bar.h"

#include "render/egl_surface.h"

#include "cursor-shape-v1-client-protocol.h"
#include "ext-idle-notify-v1-client-protocol.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "hyprland-toplevel-export-v1-client-protocol.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace backend_wayland {

namespace {

namespace output_detail {
void geometry(void *, wl_output *, int32_t, int32_t, int32_t, int32_t, int32_t, const char *, const char *, int32_t) {}
void mode(void *, wl_output *, uint32_t, int32_t, int32_t, int32_t) {}
void scale_event(void *data, wl_output *, int32_t factor) {
    static_cast<MonitorOutput *>(data)->output.scale = factor;
}
void name_event(void *data, wl_output *, const char *name) {
    static_cast<MonitorOutput *>(data)->output.name = name;
}
void description(void *, wl_output *, const char *) {}
void done(void *data, wl_output *) {
    auto *mon = static_cast<MonitorOutput *>(data);
    bool first_done = !mon->output.done;
    mon->output.done = true;
    if (!mon->activated && mon->app->egl_context != EGL_NO_CONTEXT)
        monitor_output_activate(*mon->app, *mon);
    if (first_done)
        lock_notify_output_added(*mon->app, mon->output.wl, mon->output.name.c_str());
}

const wl_output_listener &listener() {
    static constexpr wl_output_listener l{
        .geometry = geometry,
        .mode = mode,
        .done = done,
        .scale = scale_event,
        .name = name_event,
        .description = description,
    };
    return l;
}
} // namespace output_detail

namespace xdg_wm_base_listener_detail {
void ping(void *, xdg_wm_base *wm_base, uint32_t serial) {
    xdg_wm_base_pong(wm_base, serial);
}
constexpr xdg_wm_base_listener listener{.ping = ping};
} // namespace xdg_wm_base_listener_detail

void registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    auto *state = static_cast<WaylandState *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 6u)));
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell =
            static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
        xdg_wm_base_add_listener(state->wm_base, &xdg_wm_base_listener_detail::listener, nullptr);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        auto mon = std::make_unique<MonitorOutput>();
        mon->app = state;
        mon->output.registry_name = name;
        mon->output.wl = static_cast<wl_output *>(wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4u)));
        wl_output_add_listener(mon->output.wl, &output_detail::listener(), mon.get());
        state->outputs.push_back(std::move(mon));
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 3));
        state->seat_caps.keyboard = &state->keyboard;
        state->seat_caps.pointer = &state->pointer;
        keyboard_attach_seat(state->seat_caps, state->seat);
    } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
        state->idle.notifier =
            static_cast<ext_idle_notifier_v1 *>(wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, 1));
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        state->pointer.cursor_shape_manager =
            static_cast<wp_cursor_shape_manager_v1 *>(wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface, hyprland_toplevel_export_manager_v1_interface.name) == 0) {
        state->toplevel_export_manager =
            static_cast<hyprland_toplevel_export_manager_v1 *>(wl_registry_bind(registry, name, &hyprland_toplevel_export_manager_v1_interface, 1));
    } else if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
        state->text_input_manager =
            static_cast<zwp_text_input_manager_v3 *>(wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1));
    } else if (strcmp(interface, ext_session_lock_manager_v1_interface.name) == 0) {
        state->session_lock_manager =
            static_cast<ext_session_lock_manager_v1 *>(wl_registry_bind(registry, name, &ext_session_lock_manager_v1_interface, 1));
    }
}

void registry_global_remove(void *data, wl_registry *, uint32_t name) {
    auto *state = static_cast<WaylandState *>(data);
    auto it = std::find_if(state->outputs.begin(), state->outputs.end(), [name](const std::unique_ptr<MonitorOutput> &m) { return m->output.registry_name == name; });
    if (it == state->outputs.end())
        return;
    klog("output: '%s' removed", (*it)->output.name.c_str());
    wl_output *removed_wl = (*it)->output.wl;
    lock_notify_output_removed(*state, removed_wl);
    for (auto &m : state->overlays)
        m->on_output_removed(*state, removed_wl);
    if (state->last_pointer_monitor == it->get())
        state->last_pointer_monitor = nullptr;
    monitor_output_destroy(**it);
    state->outputs.erase(it);
}

constexpr wl_registry_listener kRegistryListener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

} // namespace

bool bootstrap(WaylandState &app) {
    wl_registry *registry = wl_display_get_registry(app.display);
    wl_registry_add_listener(registry, &kRegistryListener, &app);
    wl_display_roundtrip(app.display);
    wl_display_roundtrip(app.display);

    if (!app.compositor || !app.layer_shell || !app.wm_base) {
        klog("compositor is missing wl_compositor, zwlr_layer_shell_v1, or "
             "xdg_wm_base");
        return false;
    }
    if (app.outputs.empty()) {
        klog("no wl_output advertised by the compositor");
        return false;
    }
    return true;
}

} // namespace backend_wayland
