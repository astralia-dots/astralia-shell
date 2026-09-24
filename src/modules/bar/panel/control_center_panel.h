#pragma once

#include <EGL/egl.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>

#include "render/animated_image.h"
#include "render/marquee_scroll.h"
#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/brightness_service.h"
#include "service/input_service.h"
#include "service/pipewire_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

// panel layout
constexpr float kControlCenterPanelWidth = 360.0f;
constexpr float kControlCenterCardGap = 10.0f;

// profile
constexpr float kProfileAvatarSize = 100.0f;
constexpr float kProfileBorderWidth = 2.0f;
constexpr float kProfileAvatarRingWidth = 3.0f;
constexpr float kProfileRadius = 12.0f;
constexpr float kProfileVerticalPadding = 24.0f;
constexpr float kProfileTopPadding = 12.0f;
constexpr float kProfileAvatarGap = 8.0f;
constexpr float kProfileInfoSpacing = 2.0f;
constexpr float kProfileSettingsHitPadding = 8.0f;

// battery
constexpr float kBatteryBarHeight = 6.0f;
constexpr float kBatteryBarRadius = 3.0f;
constexpr float kBatteryHeaderSpacing = 10.0f;
constexpr float kBatteryRowSpacing = 8.0f;

// media
constexpr float kMediaThumbSize = 72.0f;
constexpr float kMediaThumbRadius = 8.0f;
constexpr float kMediaTitleLeftMargin = 12.0f;
constexpr float kMediaTitleSpacing = 3.0f;
constexpr float kMediaProgressRowHeight = 20.0f;
constexpr float kMediaProgressTopMargin = 10.0f;
constexpr float kMediaCtrlRowHeight = 32.0f;
constexpr float kMediaCtrlTopMargin = 8.0f;
constexpr float kMediaCtrlSpacing = 8.0f;
constexpr float kMediaSideBtnSize = 28.0f;
constexpr float kMediaSideBtnRadius = 14.0f;
constexpr float kMediaPlayBtnSize = 32.0f;
constexpr float kMediaPlayBtnRadius = 16.0f;

// volume
constexpr float kVolumeCardSpacing = 8.0f;
constexpr float kVolumeRowSpacing = 4.0f;
constexpr float kVolumeLabelRowSpacing = 4.0f;
constexpr float kVolumeDeviceTextMaxWidth = 148.0f;
constexpr float kVolumeSliderRowHeight = 24.0f;
constexpr float kVolumeCardSliderTrackHeight = 6.0f;
constexpr float kVolumeSliderPctGap = 8.0f;
constexpr float kVolumePctTextWidth = 40.0f;
constexpr float kVolumePctMuteGap = 6.0f;
constexpr float kVolumeMuteBtnSize = 22.0f;
constexpr float kVolumeMuteBtnRadius = 11.0f;

// brightness
constexpr float kBrightnessSliderRowHeight = 24.0f;
constexpr float kBrightnessSliderTrackHeight = 6.0f;
constexpr float kBrightnessSliderPctGap = 8.0f;
constexpr float kBrightnessPctTextWidth = 40.0f;
constexpr float kBrightnessIconGap = 10.0f;
constexpr float kBrightnessKeyStep = 0.01f;

struct WaylandState;

struct ControlCenterPanelState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;
    std::optional<DraggedSlider> dragging;
    std::string selected_slider_tag;
    float brightness_level = 0.0f;

    float scroll_offset = 0.0f;
    float content_height = 0.0f;
    float visible_height = 0.0f;

    AnimatedImage profile_pic;
    std::unordered_map<std::string, Texture> art_cache;
    MarqueeTextState media_title_marquee;
    MarqueeTextState media_artist_marquee;

    float pending_bar_height = 0.0f;
    float pending_bar_top_margin = 0.0f;
};

bool control_center_panel_create_surface(ControlCenterPanelState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool control_center_panel_init_egl(ControlCenterPanelState &state, Renderer &renderer, WaylandState &app, EGLDisplay display, EGLConfig config, EGLContext context);

void control_center_panel_request_frame(ControlCenterPanelState &state, float bar_height, float bar_top_margin);

void control_center_panel_toggle(ControlCenterPanelState &state);

void control_center_panel_handle_click(ControlCenterPanelState &state, WaylandState &app, double px, double py);

void control_center_panel_handle_pointer_move(ControlCenterPanelState &state, WaylandState &app, double px);

void control_center_panel_handle_scroll(ControlCenterPanelState &state, double dy);

void control_center_panel_handle_key_event(ControlCenterPanelState &state, WaylandState &app, const KeyEvent &event);

void control_center_panel_paint(ControlCenterPanelState &state, WaylandState &app, float bar_height, float bar_top_margin);
