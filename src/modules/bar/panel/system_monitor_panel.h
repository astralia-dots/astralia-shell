#pragma once

#include <EGL/egl.h>
#include <string>
#include <vector>
#include <wayland-client.h>

#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/telemetry_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

// resources card
constexpr float kGaugeDiameter = 68.0f;
constexpr float kGaugeStroke = 6.0f;
constexpr float kGaugeIconValueGap = 2.0f;
constexpr float kGaugeColumnGap = 16.0f;
constexpr float kStatsGaugeLabelSpacing = 4.0f;

// cpu temp grid card
constexpr float kTempRowHeight = 26.0f;
constexpr int kCpuCoreColumns = 4;
constexpr float kCpuCoreColumnSpacing = 8.0f;
constexpr float kCpuCoreItemHeight = 24.0f;
constexpr float kCpuCoreItemRadius = 6.0f;
constexpr float kCpuCoreRowSpacing = 6.0f;
constexpr float kCpuCoreTextMargin = 8.0f;
constexpr float kCpuTempGridTopMargin = 8.0f;

// gauge colors
constexpr const char *kGaugeColorCpuHex = "#ef4444";
constexpr const char *kGaugeColorGpuHex = "#a855f7";
constexpr const char *kGaugeColorRamHex = "#3b82f6";
constexpr const char *kGaugeColorDiskHex = "#22c55e";
constexpr const char *kTempWarnColorHex = "#f97316";

struct SystemMonitorPanelState {
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

namespace system_monitor_panel_detail {

enum class RowKind {
    ResourcesCard,
    CpuTempCard,
    GpuTempCard,
    Spacer,
};

struct PanelRow {
    RowKind kind;
    float height;
};

std::vector<PanelRow> build_rows(const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, TextureCache &tcache, int32_t scale);

float content_height(const std::vector<PanelRow> &rows);

float panel_height(const std::vector<PanelRow> &rows);

} // namespace system_monitor_panel_detail

bool system_monitor_panel_create_surface(SystemMonitorPanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool system_monitor_panel_init_egl(SystemMonitorPanelState &state, Renderer &renderer, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, EGLDisplay display, EGLConfig config, EGLContext context);

void system_monitor_panel_request_frame(SystemMonitorPanelState &state, float pill_center_x, float bar_height, float bar_top_margin);

void system_monitor_panel_toggle(SystemMonitorPanelState &state, float pill_center_x = -1.0f);

void system_monitor_panel_handle_scroll(SystemMonitorPanelState &state, const CpuTempState &cpu, const GpuTempState &gpu, const SystemStatsState &stats, double dy);

void system_monitor_panel_handle_click(SystemMonitorPanelState &state, double px, double py);

void system_monitor_panel_handle_key_event(SystemMonitorPanelState &state, const KeyEvent &event);

void system_monitor_panel_paint(SystemMonitorPanelState &state, const CpuTempState &cpu_temp, const GpuTempState &gpu_temp, const SystemStatsState &stats, float pill_center_x, float bar_height, float bar_top_margin);
