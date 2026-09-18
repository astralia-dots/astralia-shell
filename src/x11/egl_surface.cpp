#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include "x11/egl_surface.h"

#include "app/backend.h"

namespace backend_x11 {

NativeEglWindowHandle egl_native_window_create(NativeSurfaceHandle, int32_t, int32_t) {
    return nullptr;
}

EGLSurface egl_surface_create(NativeSurfaceHandle surface, NativeEglWindowHandle, EGLDisplay display, EGLConfig config) {
    auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(surface));
    return eglCreateWindowSurface(display, config, reinterpret_cast<EGLNativeWindowType>(window), nullptr);
}

void egl_native_window_resize(NativeEglWindowHandle, int32_t, int32_t) {
}

void egl_native_window_destroy(NativeEglWindowHandle) {
}

void native_surface_commit(NativeSurfaceHandle) {
}

void native_surface_set_input_region(NativeSurfaceHandle surface, void *, bool empty) {
    auto *display = static_cast<Display *>(active_display());
    auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(surface));
    if (empty)
        XShapeCombineRectangles(display, window, ShapeInput, 0, 0, nullptr, 0, ShapeSet, 0);
    else
        XShapeCombineMask(display, window, ShapeInput, 0, 0, None, ShapeSet);
}

} // namespace backend_x11
