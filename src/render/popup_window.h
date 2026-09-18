#pragma once

#include <EGL/egl.h>
#include <functional>

#include "render/egl_surface.h"
#include "render/rect.h"

#include "service/frame_service.h"
#include "service/output_service.h"

struct PopupWindowBase {
    void *compositor = nullptr;
    NativeSurfaceHandle surface = nullptr;
    void *shell_surface = nullptr;
    void *popup = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    bool configured = false, done = false;
    int32_t width = 0, height = 0;
    uint32_t reposition_token = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    std::function<void()> on_done;
};

bool popup_window_create(PopupWindowBase &base, void *compositor, void *wm_base, void *parent_layer, Rect anchor_rect, int32_t w, int32_t h, void *seat, uint32_t grab_serial);

bool popup_window_init_egl(PopupWindowBase &base, void *display, EGLDisplay egl_display, EGLConfig config, EGLContext context);

void popup_window_reposition(PopupWindowBase &base, void *wm_base, Rect anchor_rect, int32_t w, int32_t h);

void popup_window_request_frame(PopupWindowBase &base);

void popup_window_destroy(PopupWindowBase &base);
