#include "render/popup_window.h"

#include "app/backend.h"

#include "wayland/popup_window.h"
#include "x11/popup_window.h"

bool popup_window_create(PopupWindowBase &base, void *compositor, void *wm_base, void *parent_layer, Rect anchor_rect, int32_t w, int32_t h, void *seat, uint32_t grab_serial) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::popup_window_create(base, compositor, wm_base, parent_layer, anchor_rect, w, h, seat, grab_serial);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::popup_window_create(base, compositor, wm_base, parent_layer, anchor_rect, w, h, seat, grab_serial);
#else
    return false;
#endif
}

bool popup_window_init_egl(PopupWindowBase &base, void *display, EGLDisplay egl_display, EGLConfig config, EGLContext context) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::popup_window_init_egl(base, display, egl_display, config, context);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::popup_window_init_egl(base, display, egl_display, config, context);
#else
    return false;
#endif
}

void popup_window_reposition(PopupWindowBase &base, void *wm_base, Rect anchor_rect, int32_t w, int32_t h) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::popup_window_reposition(base, wm_base, anchor_rect, w, h);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::popup_window_reposition(base, wm_base, anchor_rect, w, h);
#endif
}

void popup_window_request_frame(PopupWindowBase &base) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::popup_window_request_frame(base);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::popup_window_request_frame(base);
#endif
}

void popup_window_destroy(PopupWindowBase &base) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::popup_window_destroy(base);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::popup_window_destroy(base);
#endif
}
