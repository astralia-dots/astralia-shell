#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "config/dock_config.h"

#include "render/animation.h"
#include "render/dock_row.h"
#include "render/egl_surface.h"
#include "render/layer_surface.h"
#include "render/renderer.h"
#include "render/scene.h"

#include "service/dock_service.h"
#include "service/frame_service.h"
#include "service/input_service.h"
#include "service/output_service.h"

struct wl_compositor;
struct wl_output;
struct zwlr_layer_shell_v1;

struct DockAutoHideState {
    bool enabled = false;
    bool hidden = false;
    bool collapsed = false;
    float opacity = 1.0f;
};

struct DockState {
    NativeSurfaceHandle surface = nullptr;
    wl_compositor *compositor = nullptr;
    LayerSurfaceHandle layer_surface = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    bool configured = false;
    int32_t width = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    AnimationManager animations;

    DockIconCache icons;
    DockRowState row;
    std::vector<DockEntry> entries;
    int32_t last_exclusive_zone = -1;
    std::string output_name;
    const HyprlandState *hypr = nullptr;
    const PointerState *pointer = nullptr;
    DockAutoHideState autohide;
};

bool dock_create_surface(DockState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output);

bool dock_init_egl(DockState &state, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context);

void dock_request_frame(DockState &state);

void dock_refresh(DockState &state);

void dock_apply_autohide(DockState &state, bool enabled);
