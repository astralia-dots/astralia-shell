#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <wayland-client.h>

#include "config/bar_config.h"

#include "render/node.h"
#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/pipewire_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct VolumePanelState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;
    float locked_center_x = -1.0f;
    PanelHeightReveal reveal;
    float scroll_offset = 0.0f;
    float visible_content_height = 0.0f;
    std::optional<DraggedSlider> dragging;
    std::string selected_slider_tag;

    float pending_pill_center_x = 0.0f;
    float pending_bar_height = 0.0f;
    float pending_bar_top_margin = 0.0f;
};

namespace volume_panel_detail {

enum class RowKind {
    OutputLabel,
    OutputSlider,
    InputLabel,
    InputSlider,
    Divider,
    AppsHeader,
    AppRow,
    OutputDeviceHeader,
    OutputDeviceRow,
    InputDeviceHeader,
    InputDeviceRow,
    Spacer,
};

struct PanelRow {
    RowKind kind;
    float height;
    const PwNodeEntry *entry = nullptr;
};

std::vector<PanelRow> build_rows(const PipewireState &pw);

float content_height(const std::vector<PanelRow> &rows);

float panel_height(const std::vector<PanelRow> &rows);

} // namespace volume_panel_detail

bool volume_panel_create_surface(VolumePanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool volume_panel_init_egl(VolumePanelState &state, Renderer &renderer, PipewireState &pw, EGLDisplay display, EGLConfig config, EGLContext context);

void volume_panel_request_frame(VolumePanelState &state, float pill_center_x, float bar_height, float bar_top_margin);

void volume_panel_toggle(VolumePanelState &state, float pill_center_x = -1.0f);

void volume_panel_handle_scroll(VolumePanelState &state, const PipewireState &pw, double dy);

void volume_panel_handle_pointer_move(VolumePanelState &state, PipewireState &pw, double px);

void volume_panel_handle_click(VolumePanelState &state, PipewireState &pw, double px, double py);

void volume_panel_handle_key_event(VolumePanelState &state, PipewireState &pw, const KeyEvent &event);

void volume_panel_paint(VolumePanelState &state, PipewireState &pw, float pill_center_x, float bar_height, float bar_top_margin);
