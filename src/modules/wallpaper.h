#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/config.h"

#include "config/wallpaper_config.h"

#include "render/egl_surface.h"
#include "render/layer_surface.h"
#include "render/scene.h"
#include "render/texture.h"
#include "render/video_texture.h"

#include "service/frame_service.h"
#include "service/media_service.h"
#include "service/output_service.h"

class Renderer;
struct Node;
struct WaylandState;
struct wl_compositor;
struct wl_output;
struct zwlr_layer_shell_v1;

enum class FillMode { Crop,
                      Fit };

struct WallpaperColumnGl {
    EGLDisplay display = nullptr;
    EGLContext context = nullptr;
    EGLSurface surface = EGL_NO_SURFACE;
    std::function<void()> request_frame;
};

struct WallpaperColumn {
    Texture tex;
    uint64_t generation = 0;

    unsigned char *pending_pixels = nullptr;
    int pending_width = 0;
    int pending_height = 0;
    int pending_stride = 0;

    MediaDecodePlayback decode;
    std::string path;
    FillMode mode = FillMode::Crop;
    VideoTexture video_tex;
    void *pinned_frame = nullptr;
    void *pinned_frame_prev = nullptr;
    bool zero_copy = false;

    int target_w = 0;
    int target_h = 0;

    std::shared_ptr<int> life = std::make_shared<int>(0);

    Texture tex_prev;
    WallpaperTransition pending_transition = WallpaperTransition::None;
    bool transitioning = false;
    WallpaperTransition transition_kind = WallpaperTransition::None;
    std::chrono::steady_clock::time_point transition_start{};
    float tr_direction = 0.0f;
    float tr_center_x = 0.5f;
    float tr_center_y = 0.5f;
    float tr_stripe_count = 12.0f;
    float tr_angle = 30.0f;
    float tr_cell_size = 0.04f;

    WallpaperColumn() = default;
    WallpaperColumn(const WallpaperColumn &) = delete;
    WallpaperColumn &operator=(const WallpaperColumn &) = delete;
    ~WallpaperColumn() { delete[] pending_pixels; }
};

struct WallpaperState {
    NativeSurfaceHandle surface = nullptr;
    LayerSurfaceHandle layer_surface = nullptr;
    NativeEglWindowHandle egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    WaylandState *app = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    bool configured = false;
    std::string output_name;
    int dbg_frame = 0;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;

    WallpaperColumnGl gl;
    std::vector<std::unique_ptr<WallpaperColumn>> columns;

    std::function<void()> on_resize;
};

bool wallpaper_create_surface(WallpaperState &wp, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool wallpaper_init_egl(WallpaperState &wp, Renderer &renderer, EGLDisplay display, EGLConfig config, EGLContext context);

void wallpaper_request_frame(WallpaperState &wp);

void wallpaper_wake(WallpaperState &wp);

void wallpaper_draw_columns(const WallpaperState &wp, Node *parent, int32_t width, int32_t height);

void wallpaper_sync_from_config(WallpaperState &wp, const Config &cfg, const std::string &monitor_name, bool animated);

void wallpaper_columns_stop_all(WallpaperState &wp);
void wallpaper_columns_pause_all(WallpaperState &wp);
void wallpaper_columns_resume_all(WallpaperState &wp);

MediaDecodeStatus wallpaper_column_status(const WallpaperState &wp, int column_index);
