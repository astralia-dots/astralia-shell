#include <cmath>
#include <utility>

#include "render/gl.h"
#include "render/overlay_panel.h"

void overlay_panel_configure(void *data, int32_t width, int32_t height) {
    auto *base = static_cast<OverlayPanelBase *>(data);
    base->width = width;
    base->height = height;
    int32_t scale = base->output_scale.scale;
    if (base->egl_window)
        egl_native_window_resize(base->egl_window, base->width * scale, base->height * scale);
    base->configured = true;
}

void overlay_panel_update_input_region(OverlayPanelBase &base) {
    native_surface_set_input_region(base.surface, base.compositor, !base.open);
}

bool overlay_panel_create_surface(OverlayPanelBase &base, void *compositor, void *layer_shell, const char *name_space, wl_output *output) {
    base.compositor = compositor;
    base.name_space = name_space;
    LayerSurfaceConfig cfg{
        .layer = kLayerShellOverlay,
        .name_space = name_space,
        .anchor = kLayerAnchorTop | kLayerAnchorBottom | kLayerAnchorLeft | kLayerAnchorRight,
    };
    base.layer_surface =
        layer_surface_create(base.surface, compositor, layer_shell, cfg, overlay_panel_configure, &base, output);
    if (!base.layer_surface)
        return false;

    base.output_scale.on_change = [&base](int32_t scale) {
        if (base.egl_window)
            egl_native_window_resize(base.egl_window, base.width * scale, base.height * scale);
        if (base.frame_clock.surface)
            request_frame(base.frame_clock);
    };
    output_scale_watch(base.output_scale, static_cast<wl_surface *>(base.surface));
    overlay_panel_update_input_region(base);
    native_surface_commit(base.surface);
    return true;
}

bool overlay_panel_init_egl(OverlayPanelBase &base, EGLDisplay display, EGLConfig config, EGLContext context) {
    base.egl_display = display;
    base.egl_context = context;
    int32_t scale = base.output_scale.scale;
    base.egl_window = egl_native_window_create(base.surface, base.width * scale, base.height * scale);
    base.egl_surface = egl_surface_create(base.surface, base.egl_window, display, config);
    if (base.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, base.egl_surface, context))
        return false;
    base.frame_clock.surface = base.surface;
    return true;
}

void overlay_panel_request_frame(OverlayPanelBase &base) {
    if (base.egl_surface == EGL_NO_SURFACE || !base.open)
        return;
    request_frame(base.frame_clock);
}

void overlay_panel_destroy_surface(OverlayPanelBase &base) {
    frame_clock_drop_callback(base.frame_clock);
    base.frame_clock.surface = nullptr;
    base.frame_clock.redraw_requested = false;
    base.frame_clock.mapped = false;
    if (base.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, base.egl_context);
        eglDestroySurface(base.egl_display, base.egl_surface);
        base.egl_surface = EGL_NO_SURFACE;
    }
    if (base.egl_window) {
        egl_native_window_destroy(base.egl_window);
        base.egl_window = nullptr;
    }
    NativeSurfaceHandle surface = base.surface;
    destroy_layer_surface(base.egl_display, surface, base.layer_surface, base.egl_window, base.egl_surface, nullptr);
    base.surface = surface;
    base.configured = false;
}

void overlay_panel_toggle(OverlayPanelBase &base) {
    if (!base.layer_surface || base.egl_surface == EGL_NO_SURFACE)
        return;

    bool opening = !base.open;
    if (opening) {
        base.open = true;
        layer_surface_set_keyboard_interactivity(base.layer_surface, true);
        overlay_panel_update_input_region(base);
        native_surface_commit(base.surface);
        klog("panel: %s acquired exclusive keyboard interactivity", base.name_space ? base.name_space : "?");
    }

    overlay_panel_request_frame(base);
    base.animations.animate(base.opacity, opening ? 1.0f : 0.0f, kOverlayFadeMs, Easing::EaseOutCubic, [&base](float v) { base.opacity = v; }, [&base, opening] {
            if (opening)
                return;
            base.open = false;
            layer_surface_set_keyboard_interactivity(base.layer_surface, false);
            overlay_panel_update_input_region(base);
            native_surface_commit(base.surface);
            klog("panel: %s released exclusive keyboard interactivity", base.name_space ? base.name_space : "?"); }, kOverlayFadeOwner);
}

void panel_reveal_open(PanelHeightReveal &r) {
    r.visible_height = -1.0f;
    r.target = -1.0f;
    r.closing = false;
}

float panel_reveal_tick(PanelHeightReveal &r, OverlayPanelBase &base, float target_h) {
    if (r.closing)
        return r.visible_height;

    if (r.visible_height < 0.0f) {
        r.visible_height = 0.0f;
        r.target = target_h;
        base.animations.animate(r.visible_height, target_h, kOverlayFadeMs, Easing::EaseOutCubic, [&r](float v) { r.visible_height = v; }, {}, kPanelHeightAnimOwner);
    } else if (std::fabs(target_h - r.target) > 0.5f) {
        r.target = target_h;
        base.animations.animate(r.visible_height, target_h, kOverlayFadeMs, Easing::EaseOutCubic, [&r](float v) { r.visible_height = v; }, {}, kPanelHeightAnimOwner);
    }
    return r.visible_height;
}

void panel_reveal_close(PanelHeightReveal &r, OverlayPanelBase &base, std::function<void()> on_done) {
    r.closing = true;
    base.animations.animate(r.visible_height, 0.0f, kOverlayFadeMs, Easing::EaseOutCubic, [&r](float v) { r.visible_height = v; }, [&r, on_done = std::move(on_done)] {
            r.visible_height = -1.0f;
            r.target = -1.0f;
            r.closing = false;
            if (on_done)
                on_done(); }, kPanelHeightAnimOwner);
}
