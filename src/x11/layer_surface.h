#pragma once

#include <X11/Xlib.h>

#include "render/layer_surface.h"

namespace backend_x11 {

LayerSurfaceHandle layer_surface_create(NativeSurfaceHandle &out_surface, void *compositor, void *layer_shell, const LayerSurfaceConfig &cfg, LayerSurfaceConfigureFn on_configure, void *listener_data, void *output);
void layer_surface_set_size(LayerSurfaceHandle layer_surface, int32_t width, int32_t height);
void layer_surface_set_margin(LayerSurfaceHandle layer_surface, int32_t top, int32_t right, int32_t bottom, int32_t left);
void layer_surface_set_exclusive_zone(LayerSurfaceHandle layer_surface, int32_t zone);
void layer_surface_set_keyboard_interactivity(LayerSurfaceHandle layer_surface, bool exclusive);
void layer_surface_handle_configure_notify(Window window, int32_t x, int32_t y, int32_t width, int32_t height);
void destroy_layer_surface(EGLDisplay display, NativeSurfaceHandle &surface, LayerSurfaceHandle &layer_surface, NativeEglWindowHandle &egl_window, EGLSurface &egl_surface, FrameClock *frame_clock);

} // namespace backend_x11
