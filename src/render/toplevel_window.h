#pragma once

#include <EGL/egl.h>
#include <functional>

#include "render/animation.h"
#include "render/egl_surface.h"

#include "service/frame_service.h"
#include "service/output_service.h"

struct ToplevelWindowBase {
    void *compositor = nullptr;
    NativeSurfaceHandle surface = nullptr;
    void *shell_surface = nullptr;
    void *toplevel = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    bool configured = false, open = false;
    int32_t width = 0, height = 0;
    int32_t pending_width = 0, pending_height = 0;
    int32_t last_toplevel_width = 0, last_toplevel_height = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    AnimationManager animations;
    float opacity = 0.0f;

    std::function<void()> on_close_request;
};

bool toplevel_window_create_surface(ToplevelWindowBase &base, void *compositor, void *wm_base, const char *title, const char *app_id, int32_t default_width, int32_t default_height);

bool toplevel_window_init_egl(ToplevelWindowBase &base, EGLDisplay display, EGLConfig config, EGLContext context);

void toplevel_window_request_frame(ToplevelWindowBase &base);

void toplevel_window_destroy_surface(ToplevelWindowBase &base);
