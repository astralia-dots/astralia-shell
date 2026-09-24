#pragma once

#include <EGL/egl.h>
#include <memory>
#include <string>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "app/monitor_output.h"
#include "app/per_monitor_module.h"
#include "app/wayland_state.h"

#include "modules/bar/panel/battery_panel.h"
#include "modules/bar/panel/bluetooth_panel.h"
#include "modules/bar/panel/clock_panel.h"
#include "modules/bar/panel/control_center_panel.h"
#include "modules/bar/panel/network_panel.h"
#include "modules/bar/panel/resource_panel.h"
#include "modules/bar/panel/tray_panel.h"
#include "modules/bar/panel/volume_panel.h"
#include "modules/bar/styles/geometry.h"
#include "modules/bar/widget/dock_widget.h"
#include "modules/bar/widget/status_widget.h"
#include "modules/bar/widget/widget_capsule.h"
#include "modules/bar/widget/workspace_widget.h"

#include "render/animation.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture.h"

#include "service/compositor_service.h"
#include "service/frame_service.h"
#include "service/output_service.h"
#include "service/text_input_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct AutoHideState {
    bool hidden = false;
    bool collapsed = false;
    float opacity = 1.0f;

    bool enabled = false;
};

struct BarPerMonitorState {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    int32_t width = 0;
    bool configured = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    AnimationManager animations;
    AutoHideState autohide;

    WidgetCapsuleState capsule;
    WorkspaceWidgetState workspace_widget;
    DockWidgetState dock_widget;
    NetworkPanelState network_panel;
    BluetoothPanelState bluetooth_panel;
    VolumePanelState volume_panel;
    TrayPanelState tray_panel;
    TrayMenuState tray_menu;
    BatteryPanelState battery_panel;
    ResourcePanelState resource_panel;
    ClockPanelState clock_panel;
    ControlCenterPanelState control_center_panel;

    Texture clock_texture;
    Rect clock_rect;
    Texture logout_texture;
    Texture overview_texture;
    Texture cpu_texture;
    Texture control_center_texture;
    Texture fillet_left;
    Texture fillet_right;
    Texture fillet_inner_left;
    Texture fillet_inner_right;
    int fillet_px = 0;
    int fillet_inner_px = 0;
    Texture hug_outer_left;
    Texture hug_outer_right;
    Texture hug_inner_left;
    Texture hug_inner_right;
    int hug_px = 0;
    int hug_inner_px = 0;
    int32_t applied_hug_radius_px = 0;
    StatusWidgetState status_widget;
};

class BarPerMonitorModule final : public PerMonitorModule, public TextInputClient {
  public:
    BarPerMonitorState state;

    bool create_surface(WaylandState &app, MonitorOutput &mon, wl_output *output) override;
    bool configured() const override;
    bool init_egl(WaylandState &app, MonitorOutput &mon) override;

    TextInputState text_input_state() const override;
    void text_input_apply_edit(const TextInputEdit &edit) override;
    void text_input_reset_preedit() override;
    void text_input_activated(TextInputService &) override {}
    void text_input_deactivated(TextInputService &) override;
    void destroy(WaylandState &app, MonitorOutput &mon) override;
    bool owns_surface(wl_surface *surface) const override;
    void request_frame() override;
    void apply_config(WaylandState &app, MonitorOutput &mon, const Config &new_cfg) override;
    void tick(WaylandState &app, MonitorOutput &mon) override;
    void timer_tick(WaylandState &app, MonitorOutput &mon) override;
    bool is_open() const override;
    void handle_click(WaylandState &app, MonitorOutput &mon, wl_surface *surface, int button, double x, double y, uint32_t serial) override;
    void handle_scroll(WaylandState &app, MonitorOutput &mon, wl_surface *surface, double dy) override;
    void handle_key_event(WaylandState &app, MonitorOutput &mon, const KeyEvent &event) override;
    void handle_pointer_move(WaylandState &app, MonitorOutput &mon, double x, double y) override;
    void handle_pointer_release() override;
    bool wants_pointing_hand_cursor() const override;

  private:
    MonitorOutput *mon_ = nullptr;
    int poll_tick_ = 0;
    double pointer_x_ = -1, pointer_y_ = -1;
};

BarPerMonitorState &bar_state(MonitorOutput &mon);
const BarPerMonitorState &bar_state(const MonitorOutput &mon);

inline const BarStyleSpec &bar_style_of(const MonitorOutput &mon) {
    return bar_style_spec(mon.app->cfg.bar_style);
}

inline int32_t bar_top_margin(const Config &cfg) {
    return bar_style_spec(cfg.bar_style).top_margin;
}

inline int32_t bar_hug_radius_px(const MonitorOutput &mon) {
    if (mon.app->cfg.bar_style != BarStyle::Okinami || bar_state(mon).autohide.enabled)
        return 0;
    return mon.app->compositor_state.hug_radius_px;
}

namespace bar_detail {

void bar_autohide_set_surface_geometry(zwlr_layer_surface_v1 *layer_surface, wl_surface *surface, wl_egl_window *egl_window, int32_t width, int32_t height_px, int32_t margin_top, int32_t margin_right, int32_t margin_left, int32_t exclusive_zone, int32_t output_scale);

void close_other_overlays(MonitorOutput &mon, PillId keep);

int32_t bar_current_height(const MonitorOutput &mon);

void bar_autohide_apply_geometry(MonitorOutput &mon, bool autohide, bool collapsed, const BarStyleSpec &style);

void monitor_autohide_apply(MonitorOutput &mon, bool enabled, const BarStyleSpec &style);

} // namespace bar_detail

void bar_paint(MonitorOutput &mon);
void bar_request_frame(MonitorOutput &mon);
bool bar_init_egl(MonitorOutput &mon, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context);
void dispatch_pill_click(MonitorOutput &mon, double click_x, double click_y);
void update_clock(MonitorOutput &mon);
void init_stub_widgets(MonitorOutput &mon);
