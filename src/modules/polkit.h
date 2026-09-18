#pragma once

#include <EGL/egl.h>

#include "render/marquee_scroll.h"
#include "render/overlay_panel.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/text_field.h"
#include "render/texture.h"
#include "render/texture_cache.h"

#include "service/input_service.h"

struct WaylandState;
struct wl_compositor;
struct wl_output;
struct zwlr_layer_shell_v1;

struct PolkitState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    TextFieldState password;
    TextFieldTypeAnim pw_anim;
    Texture echo_glyph;
    MarqueeTextState message_marquee;

    float card_scale = 0.0f;
    bool last_auth_error = false;
    wl_output *bound_output = nullptr;
};

bool polkit_create_surface(PolkitState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output = nullptr);

bool polkit_init_egl(PolkitState &state, Renderer &renderer, WaylandState &app, EGLDisplay display, EGLConfig config, EGLContext context);

void polkit_retarget(PolkitState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, Renderer &renderer, WaylandState &app, EGLDisplay egl_display, EGLConfig egl_config, EGLContext egl_context, wl_output *target_output, const char *target_name);

void polkit_request_frame(PolkitState &state);

void polkit_sync_open_state(PolkitState &state, WaylandState &app);

void polkit_handle_key_event(PolkitState &state, WaylandState &app, const KeyEvent &event);

void polkit_paint(PolkitState &state, WaylandState &app);
