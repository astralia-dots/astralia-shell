#include <wayland-client.h>

#include "wayland/popup_window.h"

#include "render/gl.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

namespace backend_wayland {

namespace {

xdg_positioner *make_positioner(xdg_wm_base *wm_base, Rect anchor_rect, int32_t w, int32_t h) {
    xdg_positioner *p = xdg_wm_base_create_positioner(wm_base);
    xdg_positioner_set_size(p, w, h);
    xdg_positioner_set_anchor_rect(p, static_cast<int32_t>(anchor_rect.x), static_cast<int32_t>(anchor_rect.y), static_cast<int32_t>(anchor_rect.w > 1.0f ? anchor_rect.w : 1.0f), static_cast<int32_t>(anchor_rect.h > 1.0f ? anchor_rect.h : 1.0f));
    xdg_positioner_set_anchor(p, XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
    xdg_positioner_set_gravity(p, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    xdg_positioner_set_constraint_adjustment(p, XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    return p;
}

void xdg_surface_configure(void *data, xdg_surface *surface, uint32_t serial) {
    auto *base = static_cast<PopupWindowBase *>(data);
    xdg_surface_ack_configure(surface, serial);
    base->configured = true;
    int32_t scale = base->output_scale.scale;
    if (base->egl_window)
        egl_native_window_resize(base->egl_window, base->width * scale, base->height * scale);
}

const xdg_surface_listener xdg_surface_listener_impl = {
    .configure = xdg_surface_configure,
};

void popup_configure(void *data, xdg_popup *, int32_t, int32_t, int32_t width, int32_t height) {
    auto *base = static_cast<PopupWindowBase *>(data);
    if (width > 0)
        base->width = width;
    if (height > 0)
        base->height = height;
}

void popup_done(void *data, xdg_popup *) {
    auto *base = static_cast<PopupWindowBase *>(data);
    base->done = true;
    if (base->on_done)
        base->on_done();
}

void popup_repositioned(void *, xdg_popup *, uint32_t) {}

const xdg_popup_listener xdg_popup_listener_impl = {
    .configure = popup_configure,
    .popup_done = popup_done,
    .repositioned = popup_repositioned,
};

} // namespace

bool popup_window_create(PopupWindowBase &base, void *compositor_v, void *wm_base_v, void *parent_layer_v, Rect anchor_rect, int32_t w, int32_t h, void *seat_v, uint32_t grab_serial) {
    auto *compositor = static_cast<wl_compositor *>(compositor_v);
    auto *wm_base = static_cast<xdg_wm_base *>(wm_base_v);
    auto *parent_layer = static_cast<zwlr_layer_surface_v1 *>(parent_layer_v);
    auto *seat = static_cast<wl_seat *>(seat_v);

    base.compositor = compositor;
    base.width = w;
    base.height = h;
    base.done = false;
    base.configured = false;

    auto *wl_surf = wl_compositor_create_surface(compositor);
    base.surface = wl_surf;
    auto *shell_surface = xdg_wm_base_get_xdg_surface(wm_base, wl_surf);
    base.shell_surface = shell_surface;
    if (!shell_surface) {
        wl_surface_destroy(wl_surf);
        base.surface = nullptr;
        return false;
    }
    xdg_surface_add_listener(shell_surface, &xdg_surface_listener_impl, &base);

    xdg_positioner *positioner = make_positioner(wm_base, anchor_rect, w, h);
    auto *popup = xdg_surface_get_popup(shell_surface, nullptr, positioner);
    base.popup = popup;
    xdg_positioner_destroy(positioner);
    if (!popup) {
        xdg_surface_destroy(shell_surface);
        wl_surface_destroy(wl_surf);
        base.shell_surface = nullptr;
        base.surface = nullptr;
        return false;
    }
    xdg_popup_add_listener(popup, &xdg_popup_listener_impl, &base);
    zwlr_layer_surface_v1_get_popup(parent_layer, popup);
    if (seat)
        xdg_popup_grab(popup, seat, grab_serial);

    xdg_surface_set_window_geometry(shell_surface, 0, 0, w, h);

    base.output_scale.on_change = [&base](int32_t scale) {
        if (base.egl_window)
            egl_native_window_resize(base.egl_window, base.width * scale, base.height * scale);
        if (base.frame_clock.surface)
            request_frame(base.frame_clock);
    };
    output_scale_watch(base.output_scale, wl_surf);
    wl_surface_commit(wl_surf);
    return true;
}

bool popup_window_init_egl(PopupWindowBase &base, void *display_v, EGLDisplay egl_display, EGLConfig config, EGLContext context) {
    auto *display = static_cast<wl_display *>(display_v);
    while (!base.configured)
        wl_display_dispatch(display);

    base.egl_display = egl_display;
    base.egl_context = context;
    int32_t scale = base.output_scale.scale;
    base.egl_window = egl_native_window_create(base.surface, base.width * scale, base.height * scale);
    base.egl_surface = egl_surface_create(base.surface, base.egl_window, egl_display, config);
    if (base.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(egl_display, base.egl_surface, context))
        return false;
    base.frame_clock.surface = base.surface;
    return true;
}

void popup_window_reposition(PopupWindowBase &base, void *wm_base_v, Rect anchor_rect, int32_t w, int32_t h) {
    if (!base.popup)
        return;
    auto *wm_base = static_cast<xdg_wm_base *>(wm_base_v);
    auto *popup = static_cast<xdg_popup *>(base.popup);
    xdg_positioner *positioner = make_positioner(wm_base, anchor_rect, w, h);
    xdg_popup_reposition(popup, positioner, ++base.reposition_token);
    xdg_positioner_destroy(positioner);
    xdg_surface_set_window_geometry(static_cast<xdg_surface *>(base.shell_surface), 0, 0, w, h);
    base.configured = false;
    wl_surface_commit(static_cast<wl_surface *>(base.surface));
}

void popup_window_request_frame(PopupWindowBase &base) {
    if (base.egl_surface == EGL_NO_SURFACE || base.done)
        return;
    request_frame(base.frame_clock);
}

void popup_window_destroy(PopupWindowBase &base) {
    frame_clock_drop_callback(base.frame_clock);
    base.frame_clock.surface = nullptr;
    base.frame_clock.redraw_requested = false;
    base.frame_clock.mapped = false;

    if (base.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(base.egl_display, base.egl_surface);
        base.egl_surface = EGL_NO_SURFACE;
    }
    if (base.egl_window) {
        egl_native_window_destroy(base.egl_window);
        base.egl_window = nullptr;
    }
    if (base.popup) {
        xdg_popup_destroy(static_cast<xdg_popup *>(base.popup));
        base.popup = nullptr;
    }
    if (base.shell_surface) {
        xdg_surface_destroy(static_cast<xdg_surface *>(base.shell_surface));
        base.shell_surface = nullptr;
    }
    if (base.surface) {
        wl_surface_destroy(static_cast<wl_surface *>(base.surface));
        base.surface = nullptr;
    }
    base.configured = false;
    base.done = false;
}

} // namespace backend_wayland
