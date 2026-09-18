#include <wayland-client.h>
#include <wayland-egl.h>

#include "wayland/egl_surface.h"

namespace backend_wayland {

NativeEglWindowHandle egl_native_window_create(NativeSurfaceHandle surface, int32_t width, int32_t height) {
    return wl_egl_window_create(static_cast<wl_surface *>(surface), width, height);
}

EGLSurface egl_surface_create(NativeSurfaceHandle, NativeEglWindowHandle native_window, EGLDisplay display, EGLConfig config) {
    return eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(native_window), nullptr);
}

void egl_native_window_resize(NativeEglWindowHandle native_window, int32_t width, int32_t height) {
    wl_egl_window_resize(static_cast<wl_egl_window *>(native_window), width, height, 0, 0);
}

void egl_native_window_destroy(NativeEglWindowHandle native_window) {
    wl_egl_window_destroy(static_cast<wl_egl_window *>(native_window));
}

void native_surface_commit(NativeSurfaceHandle surface) {
    wl_surface_commit(static_cast<wl_surface *>(surface));
}

void native_surface_set_input_region(NativeSurfaceHandle surface, void *compositor, bool empty) {
    auto *wl_surf = static_cast<wl_surface *>(surface);
    if (!empty) {
        wl_surface_set_input_region(wl_surf, nullptr);
        return;
    }
    wl_region *region = wl_compositor_create_region(static_cast<wl_compositor *>(compositor));
    wl_surface_set_input_region(wl_surf, region);
    wl_region_destroy(region);
}

} // namespace backend_wayland
