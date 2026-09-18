#include <wayland-client.h>

#include "wayland/session_lock.h"

#include "app/monitor_output.h"

#include "core/log.h"

#include "ext-session-lock-v1-client-protocol.h"

namespace backend_wayland {

namespace {

void surface_configure(void *data, ext_session_lock_surface_v1 *s, uint32_t serial, uint32_t w, uint32_t h) {
    auto *los = static_cast<LockOutputSurface *>(data);
    ext_session_lock_surface_v1_ack_configure(s, serial);

    LockState *st = los->owner;
    int32_t scale = los->output_scale.scale;
    bool first = los->egl_surface == EGL_NO_SURFACE;
    los->width = static_cast<int32_t>(w);
    los->height = static_cast<int32_t>(h);

    if (first) {
        los->egl_window = egl_native_window_create(los->surface, los->width * scale, los->height * scale);
        los->egl_surface = egl_surface_create(los->surface, los->egl_window, st->app->egl_display, st->app->egl_config);
        if (los->egl_surface == EGL_NO_SURFACE) {
            klog("lock: eglCreateWindowSurface failed on '%s'", los->output_name.c_str());
            return;
        }
        los->frame_clock.surface = los->surface;
        los->frame_clock.draw = [st, los] { lock_paint(*st, *los); };
    } else if (los->egl_window) {
        egl_native_window_resize(los->egl_window, los->width * scale, los->height * scale);
    }
    los->configured = true;
    request_frame(los->frame_clock);
    app_detail::rest_egl_current(*st->app);
}

constexpr ext_session_lock_surface_v1_listener kSurfaceListener = {
    .configure = surface_configure,
};

void handle_locked(void *data, ext_session_lock_v1 *) {
    auto *st = static_cast<LockState *>(data);
    if (!st->active)
        return;
    st->locked = true;
    st->locked_at = std::chrono::steady_clock::now();
    if (st->app)
        st->app->session_locked = true;
    klog("lock: session locked, %zu surface(s)", st->surfaces.size());
    for (auto &up : st->surfaces)
        klog("lock:   '%s' configured=%d egl=%d %dx%d", up->output_name.c_str(), up->configured, up->egl_surface != EGL_NO_SURFACE, up->width, up->height);
    lock_request_all_frames(*st);
}

void handle_finished(void *data, ext_session_lock_v1 *) {
    auto *st = static_cast<LockState *>(data);
    if (!st->lock)
        return;
    klog("lock: compositor sent finished");
    if (st->locked)
        ext_session_lock_v1_unlock_and_destroy(st->lock);
    else
        ext_session_lock_v1_destroy(st->lock);
    st->lock = nullptr;
    st->locked = false;
    lock_teardown(*st);
}

constexpr ext_session_lock_v1_listener kLockListener = {
    .locked = handle_locked,
    .finished = handle_finished,
};

} // namespace

ext_session_lock_v1 *session_lock_acquire(WaylandState &app, LockState &st) {
    if (!app.session_lock_manager) {
        klog("lock: compositor has no ext_session_lock_manager_v1");
        return nullptr;
    }
    ext_session_lock_v1 *lock = ext_session_lock_manager_v1_lock(app.session_lock_manager);
    if (!lock) {
        klog("lock: failed to create session lock");
        return nullptr;
    }
    ext_session_lock_v1_add_listener(lock, &kLockListener, &st);
    return lock;
}

bool session_lock_create_surface(LockState &st, LockOutputSurface &los, wl_output *output) {
    los.surface = wl_compositor_create_surface(st.app->compositor);
    los.lock_surface = ext_session_lock_v1_get_lock_surface(st.lock, los.surface, output);
    if (!los.lock_surface) {
        klog("lock: get_lock_surface failed on '%s'", los.output_name.c_str());
        wl_surface_destroy(los.surface);
        return false;
    }
    ext_session_lock_surface_v1_add_listener(los.lock_surface, &kSurfaceListener, &los);
    los.output_scale.on_change = [&los](int32_t s) {
        if (los.egl_window)
            egl_native_window_resize(los.egl_window, los.width * s, los.height * s);
        if (los.frame_clock.surface)
            request_frame(los.frame_clock);
    };
    output_scale_watch(los.output_scale, los.surface);
    return true;
}

void session_lock_destroy_surface(LockOutputSurface &los) {
    if (los.lock_surface) {
        ext_session_lock_surface_v1_destroy(los.lock_surface);
        los.lock_surface = nullptr;
    }
    if (los.surface) {
        wl_surface_destroy(los.surface);
        los.surface = nullptr;
    }
}

void session_lock_release(LockState &st) {
    ext_session_lock_v1_unlock_and_destroy(st.lock);
    wl_display_roundtrip(st.app->display);
}

} // namespace backend_wayland
