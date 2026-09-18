#include "render/egl_surface.h"

#include "app/backend.h"

#include "wayland/egl_surface.h"
#include "x11/egl_surface.h"

NativeEglWindowHandle egl_native_window_create(NativeSurfaceHandle surface, int32_t width, int32_t height) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::egl_native_window_create(surface, width, height);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::egl_native_window_create(surface, width, height);
#else
    return nullptr;
#endif
}

EGLSurface egl_surface_create(NativeSurfaceHandle surface, NativeEglWindowHandle native_window, EGLDisplay display, EGLConfig config) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::egl_surface_create(surface, native_window, display, config);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::egl_surface_create(surface, native_window, display, config);
#else
    return EGL_NO_SURFACE;
#endif
}

void egl_native_window_resize(NativeEglWindowHandle native_window, int32_t width, int32_t height) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::egl_native_window_resize(native_window, width, height);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::egl_native_window_resize(native_window, width, height);
#endif
}

void egl_native_window_destroy(NativeEglWindowHandle native_window) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::egl_native_window_destroy(native_window);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::egl_native_window_destroy(native_window);
#endif
}

void native_surface_commit(NativeSurfaceHandle surface) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::native_surface_commit(surface);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::native_surface_commit(surface);
#endif
}

void native_surface_set_input_region(NativeSurfaceHandle surface, void *compositor, bool empty) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::native_surface_set_input_region(surface, compositor, empty);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::native_surface_set_input_region(surface, compositor, empty);
#endif
}
