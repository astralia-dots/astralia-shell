#include "render/toplevel_window.h"

#include "app/backend.h"

#include "wayland/toplevel_window.h"
#include "x11/toplevel_window.h"

bool toplevel_window_create_surface(ToplevelWindowBase &base, void *compositor, void *wm_base, const char *title, const char *app_id, int32_t default_width, int32_t default_height) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::toplevel_window_create_surface(base, compositor, wm_base, title, app_id, default_width, default_height);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::toplevel_window_create_surface(base, compositor, wm_base, title, app_id, default_width, default_height);
#else
    return false;
#endif
}

bool toplevel_window_init_egl(ToplevelWindowBase &base, EGLDisplay display, EGLConfig config, EGLContext context) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::toplevel_window_init_egl(base, display, config, context);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::toplevel_window_init_egl(base, display, config, context);
#else
    return false;
#endif
}

void toplevel_window_request_frame(ToplevelWindowBase &base) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::toplevel_window_request_frame(base);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::toplevel_window_request_frame(base);
#endif
}

void toplevel_window_destroy_surface(ToplevelWindowBase &base) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::toplevel_window_destroy_surface(base);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::toplevel_window_destroy_surface(base);
#endif
}
