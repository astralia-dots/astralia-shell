#include "render/layer_surface.h"

#include "app/backend.h"

#include "wayland/layer_surface.h"
#include "x11/layer_surface.h"

LayerSurfaceHandle layer_surface_create(NativeSurfaceHandle &out_surface, void *compositor, void *layer_shell, const LayerSurfaceConfig &cfg, LayerSurfaceConfigureFn on_configure, void *listener_data, void *output) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::layer_surface_create(out_surface, compositor, layer_shell, cfg, on_configure, listener_data, output);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::layer_surface_create(out_surface, compositor, layer_shell, cfg, on_configure, listener_data, output);
#else
    return nullptr;
#endif
}

void layer_surface_set_size(LayerSurfaceHandle layer_surface, int32_t width, int32_t height) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::layer_surface_set_size(layer_surface, width, height);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::layer_surface_set_size(layer_surface, width, height);
#endif
}

void layer_surface_set_margin(LayerSurfaceHandle layer_surface, int32_t top, int32_t right, int32_t bottom, int32_t left) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::layer_surface_set_margin(layer_surface, top, right, bottom, left);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::layer_surface_set_margin(layer_surface, top, right, bottom, left);
#endif
}

void layer_surface_set_exclusive_zone(LayerSurfaceHandle layer_surface, int32_t zone) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::layer_surface_set_exclusive_zone(layer_surface, zone);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::layer_surface_set_exclusive_zone(layer_surface, zone);
#endif
}

void layer_surface_set_keyboard_interactivity(LayerSurfaceHandle layer_surface, bool exclusive) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::layer_surface_set_keyboard_interactivity(layer_surface, exclusive);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::layer_surface_set_keyboard_interactivity(layer_surface, exclusive);
#endif
}

void destroy_layer_surface(EGLDisplay display, NativeSurfaceHandle &surface, LayerSurfaceHandle &layer_surface, NativeEglWindowHandle &egl_window, EGLSurface &egl_surface, FrameClock *frame_clock) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::destroy_layer_surface(display, surface, layer_surface, egl_window, egl_surface, frame_clock);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::destroy_layer_surface(display, surface, layer_surface, egl_window, egl_surface, frame_clock);
#endif
}
