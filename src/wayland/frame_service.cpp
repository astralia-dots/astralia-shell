#include <wayland-client.h>

#include "wayland/frame_service.h"

namespace backend_wayland {

namespace {

const wl_callback_listener &listener();

void arm_callback(FrameClock &clock) {
    wl_callback *cb = wl_surface_frame(static_cast<wl_surface *>(clock.surface));
    clock.callback = cb;
    wl_callback_add_listener(cb, &listener(), &clock);
}

void frame_done(void *data, wl_callback *cb, uint32_t) {
    auto *clock = static_cast<FrameClock *>(data);
    wl_callback_destroy(cb);
    clock->callback = nullptr;
    if (clock->redraw_requested) {
        clock->redraw_requested = false;
        arm_callback(*clock);
        clock->draw();
    }
}

const wl_callback_listener &listener() {
    static constexpr wl_callback_listener l{.done = frame_done};
    return l;
}

} // namespace

void frame_clock_drop_callback(FrameClock &clock) {
    if (clock.callback) {
        wl_callback_destroy(static_cast<wl_callback *>(clock.callback));
        clock.callback = nullptr;
    }
    clock.redraw_requested = false;
}

void request_frame(FrameClock &clock) {
    if (clock.callback) {
        clock.redraw_requested = true;
        return;
    }
    if (!clock.mapped) {
        clock.mapped = true;
        arm_callback(clock);
        clock.draw();
        return;
    }
    clock.redraw_requested = true;
    arm_callback(clock);
    wl_surface_damage_buffer(static_cast<wl_surface *>(clock.surface), 0, 0, 1, 1);
    wl_surface_commit(static_cast<wl_surface *>(clock.surface));
}

} // namespace backend_wayland
