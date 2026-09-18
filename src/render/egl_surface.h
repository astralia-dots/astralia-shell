#pragma once

#include <EGL/egl.h>
#include <cstdint>

using NativeSurfaceHandle = void *;

using NativeEglWindowHandle = void *;

NativeEglWindowHandle egl_native_window_create(NativeSurfaceHandle surface, int32_t width, int32_t height);

EGLSurface egl_surface_create(NativeSurfaceHandle surface, NativeEglWindowHandle native_window, EGLDisplay display, EGLConfig config);

void egl_native_window_resize(NativeEglWindowHandle native_window, int32_t width, int32_t height);

void egl_native_window_destroy(NativeEglWindowHandle native_window);

void native_surface_commit(NativeSurfaceHandle surface);

void native_surface_set_input_region(NativeSurfaceHandle surface, void *compositor, bool empty);
