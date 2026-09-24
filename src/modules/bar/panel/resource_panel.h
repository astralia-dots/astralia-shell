#pragma once

#include <EGL/egl.h>
#include <string>
#include <vector>
#include <wayland-client.h>

#include "config/bar_config.h"

#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/telemetry_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct ResourcePanelState {
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

    float pending_pill_center_x = 0.0f;
    float pending_bar_height = 0.0f;
    float pending_bar_top_margin = 0.0f;
};

namespace resource_panel_detail {

enum class RowKind {
    DeviceCards,
    MemoryCard,
    Spacer,
};

struct PanelRow {
    RowKind kind;
    float height;
};

std::vector<PanelRow> build_rows(const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, TextureCache &tcache, int32_t scale);

float content_height(const std::vector<PanelRow> &rows);

float panel_height(const std::vector<PanelRow> &rows);

} // namespace resource_panel_detail

bool resource_panel_create_surface(ResourcePanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool resource_panel_init_egl(ResourcePanelState &state, Renderer &renderer, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, EGLDisplay display, EGLConfig config, EGLContext context);

void resource_panel_request_frame(ResourcePanelState &state, float pill_center_x, float bar_height, float bar_top_margin);

void resource_panel_toggle(ResourcePanelState &state, float pill_center_x = -1.0f);

void resource_panel_handle_scroll(ResourcePanelState &state, const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, double dy);

void resource_panel_handle_click(ResourcePanelState &state, double px, double py);

void resource_panel_handle_key_event(ResourcePanelState &state, const KeyEvent &event);

void resource_panel_paint(ResourcePanelState &state, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, float pill_center_x, float bar_height, float bar_top_margin);
