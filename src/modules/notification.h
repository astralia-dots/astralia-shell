#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config/notification_config.h"

#include "render/animation.h"
#include "render/egl_surface.h"
#include "render/layer_surface.h"
#include "render/palette.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture.h"

#include "service/frame_service.h"
#include "service/notification_service.h"
#include "service/output_service.h"

struct wl_compositor;
struct wl_output;
struct zwlr_layer_shell_v1;

struct NotificationEntry {
    uint32_t id = 0;
    std::string app_name;
    std::string summary;
    std::string body;
    bool content_built = false;
    Texture app_name_texture;
    Texture summary_texture;
    Texture body_texture;
    uint8_t urgency = 1;
    int32_t timeout_ms = 5000;
    float height = 0.0f;
    float opacity = 0.0f;
    float slide_offset = kNotificationSlideOffset;
    float progress = 1.0f;
    bool exiting = false;
};

struct NotificationRenderModel {
    std::vector<NotificationEntry> entries;
    AnimationManager animations;
};

struct NotificationView {
    NativeSurfaceHandle surface = nullptr;
    LayerSurfaceHandle layer_surface = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    wl_compositor *compositor = nullptr;
    Renderer *renderer = nullptr;
    bool configured = false;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    std::vector<std::pair<uint32_t, Rect>> close_hitboxes;
    uint32_t hovered_close_id = 0;
    AnimationManager local_animations;
    std::unordered_map<uint32_t, float> local_exit;
};

float notification_detail_texture_height(const Texture &tex);

const Color &notification_detail_urgency_color(uint8_t urgency);

bool notification_view_create_surface(NotificationView &view, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool notification_view_init_egl(NotificationView &view, NotificationRenderModel &service, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context);

void notification_view_request_frame(NotificationView &view);

void notification_sync(NotificationRenderModel &service, const NotificationService &notifications);

bool notification_view_handle_close_click(NotificationView &view, double x, double y);

bool notification_view_set_close_hover(NotificationView &view, double x, double y);

bool notification_view_clear_close_hover(NotificationView &view);

void notification_paint(NotificationView &view, NotificationRenderModel &service);
