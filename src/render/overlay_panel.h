#pragma once

#include <EGL/egl.h>
#include <functional>

#include "app/backend.h"

#include "core/log.h"

#include "render/animation.h"
#include "render/egl_surface.h"
#include "render/layer_surface.h"

#include "service/frame_service.h"
#include "service/output_service.h"

struct wl_output;

constexpr float kOverlayFadeMs = 220.0f;
constexpr uint64_t kOverlayFadeOwner = 1;
constexpr uint64_t kPanelHeightAnimOwner = 2;

struct OverlayPanelBase {
    const char *name_space = nullptr;
    void *compositor = nullptr;
    NativeSurfaceHandle surface = nullptr;
    LayerSurfaceHandle layer_surface = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    bool configured = false, open = false;
    int32_t width = 0, height = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    AnimationManager animations;
    float opacity = 0.0f;
};

void overlay_panel_configure(void *data, int32_t width, int32_t height);

void overlay_panel_update_input_region(OverlayPanelBase &base);

bool overlay_panel_create_surface(OverlayPanelBase &base, void *compositor, void *layer_shell, const char *name_space, wl_output *output = nullptr);

bool overlay_panel_init_egl(OverlayPanelBase &base, EGLDisplay display, EGLConfig config, EGLContext context);

void overlay_panel_request_frame(OverlayPanelBase &base);

void overlay_panel_toggle(OverlayPanelBase &base);

void overlay_panel_destroy_surface(OverlayPanelBase &base);

inline void overlay_panel_release_output(OverlayPanelBase &base, wl_output *&bound_output, wl_output *removed) {
    if (!removed || bound_output != removed)
        return;
    overlay_panel_destroy_surface(base);
    base.open = false;
    base.opacity = 0.0f;
    base.animations = {};
    bound_output = nullptr;
}

struct PanelHeightReveal {
    float visible_height = -1.0f;
    float target = -1.0f;
    bool closing = false;
};

void panel_reveal_open(PanelHeightReveal &r);

float panel_reveal_tick(PanelHeightReveal &r, OverlayPanelBase &base, float target_h);

void panel_reveal_close(PanelHeightReveal &r, OverlayPanelBase &base, std::function<void()> on_done);

template <typename CreateSurface, typename InitEgl>
inline bool overlay_panel_ensure(OverlayPanelBase &base, CreateSurface create_surface, InitEgl init_egl) {
    if (base.layer_surface)
        return true;
    if (!create_surface())
        return false;
    while (!base.configured)
        backend_wait_dispatch();
    return init_egl();
}

template <typename CreateSurface, typename InitEgl>
inline wl_output *
overlay_panel_retarget(OverlayPanelBase &base, wl_output *previous_output, wl_output *target_output, const char *target_name, CreateSurface create_surface, InitEgl init_egl) {
    klog("panel: %s retargeting from output=%p to '%s'", base.name_space ? base.name_space : "?", static_cast<void *>(previous_output), target_name);
    overlay_panel_destroy_surface(base);
    base.configured = false;
    base.open = false;
    base.opacity = 0.0f;

    auto bind_to = [&](wl_output *out) -> bool {
        if (!create_surface(out))
            return false;
        while (!base.configured)
            backend_wait_dispatch();
        return init_egl();
    };

    if (bind_to(target_output))
        return target_output;
    if (previous_output && bind_to(previous_output))
        return previous_output;
    klog("panel: %s retarget fallback also failed", base.name_space ? base.name_space : "?");
    return nullptr;
}

template <typename OnOpen, typename OnClose>
inline bool panel_lock_toggle(OverlayPanelBase &base, float &locked_center_x, float pill_center_x, OnOpen on_open, OnClose on_close) {
    bool was_open = base.open;
    overlay_panel_toggle(base);
    if (was_open) {
        on_close();
    } else {
        if (pill_center_x >= 0.0f)
            locked_center_x = pill_center_x;
        on_open();
    }
    return !was_open;
}
