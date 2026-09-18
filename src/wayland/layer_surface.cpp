#include <wayland-client.h>

#include "wayland/layer_surface.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace backend_wayland {

namespace {

struct WaylandLayerSurface {
    zwlr_layer_surface_v1 *raw = nullptr;
    LayerSurfaceConfigureFn on_configure = nullptr;
    void *data = nullptr;
};

void configure_cb(void *data, zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width, uint32_t height) {
    auto *wrapper = static_cast<WaylandLayerSurface *>(data);
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    if (wrapper->on_configure)
        wrapper->on_configure(wrapper->data, static_cast<int32_t>(width), static_cast<int32_t>(height));
}

void closed_cb(void *, zwlr_layer_surface_v1 *) {}

constexpr zwlr_layer_surface_v1_listener kListener = {
    .configure = configure_cb,
    .closed = closed_cb,
};

} // namespace

LayerSurfaceHandle layer_surface_create(NativeSurfaceHandle &out_surface, void *compositor_v, void *layer_shell_v, const LayerSurfaceConfig &cfg, LayerSurfaceConfigureFn on_configure, void *listener_data, void *output_v) {
    auto *compositor = static_cast<wl_compositor *>(compositor_v);
    auto *layer_shell = static_cast<zwlr_layer_shell_v1 *>(layer_shell_v);
    auto *output = static_cast<wl_output *>(output_v);

    auto *wl_surf = wl_compositor_create_surface(compositor);
    out_surface = wl_surf;

    zwlr_layer_surface_v1 *layer_surface =
        zwlr_layer_shell_v1_get_layer_surface(layer_shell, wl_surf, output, cfg.layer, cfg.name_space);
    if (!layer_surface)
        return nullptr;

    if (cfg.anchor)
        zwlr_layer_surface_v1_set_anchor(layer_surface, cfg.anchor);
    if (cfg.width || cfg.height)
        zwlr_layer_surface_v1_set_size(layer_surface, cfg.width, cfg.height);
    if (cfg.margin_top || cfg.margin_right || cfg.margin_bottom || cfg.margin_left)
        zwlr_layer_surface_v1_set_margin(layer_surface, cfg.margin_top, cfg.margin_right, cfg.margin_bottom, cfg.margin_left);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, cfg.exclusive_zone);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    auto *wrapper = new WaylandLayerSurface{layer_surface, on_configure, listener_data};
    zwlr_layer_surface_v1_add_listener(layer_surface, &kListener, wrapper);

    if (cfg.empty_input_region) {
        wl_region *empty_region = wl_compositor_create_region(compositor);
        wl_surface_set_input_region(wl_surf, empty_region);
        wl_region_destroy(empty_region);
    }

    return wrapper;
}

void layer_surface_set_size(LayerSurfaceHandle layer_surface, int32_t width, int32_t height) {
    zwlr_layer_surface_v1_set_size(static_cast<WaylandLayerSurface *>(layer_surface)->raw, width, height);
}

void layer_surface_set_margin(LayerSurfaceHandle layer_surface, int32_t top, int32_t right, int32_t bottom, int32_t left) {
    zwlr_layer_surface_v1_set_margin(static_cast<WaylandLayerSurface *>(layer_surface)->raw, top, right, bottom, left);
}

void layer_surface_set_exclusive_zone(LayerSurfaceHandle layer_surface, int32_t zone) {
    zwlr_layer_surface_v1_set_exclusive_zone(static_cast<WaylandLayerSurface *>(layer_surface)->raw, zone);
}

void layer_surface_set_keyboard_interactivity(LayerSurfaceHandle layer_surface, bool exclusive) {
    zwlr_layer_surface_v1_set_keyboard_interactivity(static_cast<WaylandLayerSurface *>(layer_surface)->raw, exclusive ? ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE : ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
}

void destroy_layer_surface(EGLDisplay display, NativeSurfaceHandle &surface, LayerSurfaceHandle &layer_surface, NativeEglWindowHandle &egl_window, EGLSurface &egl_surface, FrameClock *frame_clock) {
    if (frame_clock)
        frame_clock_drop_callback(*frame_clock);
    if (egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, egl_surface);
        egl_surface = EGL_NO_SURFACE;
    }
    if (egl_window) {
        egl_native_window_destroy(egl_window);
        egl_window = nullptr;
    }
    if (layer_surface) {
        auto *wrapper = static_cast<WaylandLayerSurface *>(layer_surface);
        zwlr_layer_surface_v1_destroy(wrapper->raw);
        delete wrapper;
        layer_surface = nullptr;
    }
    if (surface) {
        wl_surface_destroy(static_cast<wl_surface *>(surface));
        surface = nullptr;
    }
}

} // namespace backend_wayland
