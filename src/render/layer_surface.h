#pragma once

#include <EGL/egl.h>
#include <cstdint>

#include "render/egl_surface.h"

#include "service/frame_service.h"

constexpr uint32_t kLayerShellBackground = 0;
constexpr uint32_t kLayerShellBottom = 1;
constexpr uint32_t kLayerShellTop = 2;
constexpr uint32_t kLayerShellOverlay = 3;

constexpr uint32_t kLayerAnchorTop = 1;
constexpr uint32_t kLayerAnchorBottom = 2;
constexpr uint32_t kLayerAnchorLeft = 4;
constexpr uint32_t kLayerAnchorRight = 8;

struct LayerSurfaceConfig {
    uint32_t layer;
    const char *name_space;
    uint32_t anchor = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t margin_top = 0;
    int32_t margin_right = 0;
    int32_t margin_bottom = 0;
    int32_t margin_left = 0;
    int32_t exclusive_zone = -1;
    bool empty_input_region = false;
};

using LayerSurfaceHandle = void *;

using LayerSurfaceConfigureFn = void (*)(void *data, int32_t width, int32_t height);

LayerSurfaceHandle layer_surface_create(NativeSurfaceHandle &out_surface, void *compositor, void *layer_shell, const LayerSurfaceConfig &cfg, LayerSurfaceConfigureFn on_configure, void *listener_data, void *output = nullptr);

void layer_surface_set_size(LayerSurfaceHandle layer_surface, int32_t width, int32_t height);

void layer_surface_set_margin(LayerSurfaceHandle layer_surface, int32_t top, int32_t right, int32_t bottom, int32_t left);

void layer_surface_set_exclusive_zone(LayerSurfaceHandle layer_surface, int32_t zone);

void layer_surface_set_keyboard_interactivity(LayerSurfaceHandle layer_surface, bool exclusive);

void destroy_layer_surface(EGLDisplay display, NativeSurfaceHandle &surface, LayerSurfaceHandle &layer_surface, NativeEglWindowHandle &egl_window, EGLSurface &egl_surface, FrameClock *frame_clock = nullptr);
