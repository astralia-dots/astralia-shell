#include <chrono>
#include <cstdint>
#include <string>

#include "modules/polkit.h"

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "render/gl.h"
#include "render/image.h"
#include "render/marquee_text.h"
#include "render/node.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

constexpr const char *kPolkitNamespace = "adastria-shell-polkit";

constexpr float kPolkitAnimMs = 350.0f;
constexpr float kPolkitScaleHidden = 0.0f;
constexpr float kPolkitBorderWidth = 5.0f;
constexpr float kPolkitCardPad = 30.0f;
constexpr float kPolkitCardRadius = 20.0f;
constexpr float kPolkitCardWidth = 480.0f;
constexpr float kPolkitDotMargin = 8.0f;
constexpr float kPolkitDotSize = 16.0f;
constexpr float kPolkitFieldHeight = 55.0f;
constexpr float kPolkitFieldRadius = 27.0f;
constexpr float kPolkitSpacing = 16.0f;

constexpr float kPolkitTitleLineH = 20.0f;
constexpr float kPolkitMessageLineH = 16.0f;
constexpr float kPolkitSupplementaryLineH = 14.0f;

constexpr uint64_t kPolkitCardScaleOwner = 10;
constexpr uint64_t kPolkitDotAnimBase = 1000;

const Texture *tc_text(PolkitState &st, const std::string &s, int px, bool bold, int32_t scale, int max_width_px = 0) {
    if (s.empty())
        return nullptr;
    std::string key = "pt:" + std::to_string(px) + (bold ? "b:" : "n:") + std::to_string(max_width_px) + ":" + s;
    return st.tcache.get(key, [&] {
        return rasterize_text_px(s, px, bold, scale, max_width_px);
    });
}

float px_w(const Texture *t) {
    if (!t)
        return 0.0f;
    return static_cast<float>(t->width) / static_cast<float>(t->scale > 0 ? t->scale : 1);
}
float px_h(const Texture *t) {
    if (!t)
        return 0.0f;
    return static_cast<float>(t->height) / static_cast<float>(t->scale > 0 ? t->scale : 1);
}

size_t utf8_len(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80)
            ++n;
    return n;
}

void animate_card(PolkitState &state, bool opening) {
    overlay_panel_request_frame(state.base);
    state.base.animations.animate(state.card_scale, opening ? 1.0f : kPolkitScaleHidden, kPolkitAnimMs, opening ? Easing::EaseOutBack : Easing::EaseInBack, [&state](float v) { state.card_scale = v; }, [&state, opening] {
            if (opening)
                return;
            state.base.open = false;
            zwlr_layer_surface_v1_set_keyboard_interactivity(state.base.layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
            overlay_panel_update_input_region(state.base);
            wl_surface_commit(state.base.surface); }, kPolkitCardScaleOwner);
}

void open_card(PolkitState &state) {
    if (!state.base.layer_surface || state.base.egl_surface == EGL_NO_SURFACE)
        return;
    state.base.open = true;
    state.base.opacity = 1.0f;
    zwlr_layer_surface_v1_set_keyboard_interactivity(state.base.layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    overlay_panel_update_input_region(state.base);
    wl_surface_commit(state.base.surface);
    animate_card(state, true);
}

void close_card(PolkitState &state) {
    if (!state.base.open)
        return;
    state.password.text.clear();
    state.password.error_message.clear();
    state.last_auth_error = false;
    text_field_type_anim_clear(state.pw_anim, state.base.animations, kPolkitDotAnimBase);
    animate_card(state, false);
}

} // namespace

bool polkit_create_surface(PolkitState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell, kPolkitNamespace, output);
}

bool polkit_init_egl(PolkitState &state, Renderer &renderer, WaylandState &app, EGLDisplay display, EGLConfig config, EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state, &app] { polkit_paint(state, app); };
    state.echo_glyph = load_image_texture_first_existing({ADASTRIA_SHELL_INPUT_ECHO, "assets/electro.png"});
    return true;
}

void polkit_retarget(PolkitState &state, wl_compositor *compositor, zwlr_layer_shell_v1 *layer_shell, wl_display *display, Renderer &renderer, WaylandState &app, EGLDisplay egl_display, EGLConfig egl_config, EGLContext egl_context, wl_output *target_output, const char *target_name) {
    wl_output *bound = overlay_panel_retarget(state.base, display, state.bound_output, target_output, target_name, [&](wl_output *out) { return polkit_create_surface(state, compositor, layer_shell, out); }, [&] { return polkit_init_egl(state, renderer, app, egl_display, egl_config, egl_context); });
    if (bound)
        state.bound_output = bound;
}

void polkit_request_frame(PolkitState &state) {
    overlay_panel_request_frame(state.base);
}

void polkit_sync_open_state(PolkitState &state, WaylandState &app) {
    bool pending = app.polkit.has_pending_request();
    if (pending && !state.base.open) {
        MonitorOutput *target = app_detail::active_target_monitor(app);
        if (target && (target->output.wl != state.bound_output || !state.base.layer_surface))
            polkit_retarget(state, app.compositor, app.layer_shell, app.display, app.renderer, app, app.egl_display, app.egl_config, app.egl_context, target->output.wl, target->output.name.c_str());
        open_card(state);
        return;
    }
    if (!pending && state.base.open) {
        close_card(state);
        return;
    }
    polkit_request_frame(state);
}

void polkit_handle_key_event(PolkitState &state, WaylandState &app, const KeyEvent &event) {
    TextFieldResult res = text_field_handle_key(state.password, event);
    switch (res) {
    case TextFieldResult::Changed:
        text_field_type_anim_sync(state.pw_anim, state.base.animations, kPolkitDotAnimBase, state.password.text);
        polkit_request_frame(state);
        break;
    case TextFieldResult::Committed:
        if (!state.password.text.empty())
            app.polkit.submit_response(state.password.text);
        state.password.text.clear();
        text_field_type_anim_clear(state.pw_anim, state.base.animations, kPolkitDotAnimBase);
        polkit_request_frame(state);
        break;
    case TextFieldResult::Cancelled:
        app.polkit.cancel_request();
        break;
    case TextFieldResult::None:
        break;
    }
}

void polkit_paint(PolkitState &state, WaylandState &app) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    gl_make_current(state.base.egl_display, state.base.egl_surface, state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    if (!state.base.open) {
        state.scene.draw(*state.renderer);
        eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
        return;
    }

    const PolkitRequest request = app.polkit.pending_request();
    bool needs_input = app.polkit.is_response_required();
    std::string supplementary = app.polkit.supplementary_message();
    bool supplementary_is_error = app.polkit.supplementary_is_error();
    if (supplementary_is_error && !state.last_auth_error)
        state.password.error_message = "Skill Issue";
    state.last_auth_error = supplementary_is_error;
    bool show_supplementary = !supplementary.empty() && !supplementary_is_error;

    Node *root = &state.scene.root;

    float card_h = kPolkitCardPad * 2.0f + kPolkitTitleLineH + kPolkitSpacing + kPolkitMessageLineH + kPolkitSpacing + kPolkitFieldHeight + (show_supplementary ? kPolkitSpacing + kPolkitSupplementaryLineH : 0.0f);
    float card_x =
        (static_cast<float>(state.base.width) - kPolkitCardWidth) * 0.5f;
    float card_y = (static_cast<float>(state.base.height) - card_h) * 0.5f;

    Node *card = node_add_rrect(root, card_x, card_y, kPolkitCardWidth, card_h, kPolkitCardRadius, kPolkitBorderWidth, rgba(palette::overlay), rgba(palette::accent));
    card->scale = state.card_scale;

    float content_x = kPolkitCardPad;
    float content_w = kPolkitCardWidth - kPolkitCardPad * 2.0f;
    float content_cx = content_x + content_w * 0.5f;
    float y = kPolkitCardPad;

    int content_w_px = static_cast<int>(content_w);
    const Texture *title_t = tc_text(state, "Authentication Required", 15, true, scale, content_w_px);
    if (title_t)
        node_add_texture_rect(card, content_x, y, px_w(title_t), px_h(title_t), *title_t, rgba(palette::text));
    y += kPolkitTitleLineH + kPolkitSpacing;

    std::string message =
        request.message.empty() ? request.action_id : request.message;
    draw_marquee_text(card, state.tcache, state.base.animations, state.message_marquee, scale, message, content_x, y, content_w, rgba(palette::text_muted));
    y += kPolkitMessageLineH + kPolkitSpacing;

    node_add_rrect(card, content_x, y, content_w, kPolkitFieldHeight, kPolkitFieldRadius, kPolkitBorderWidth, rgba(palette::field_bg), rgba(palette::accent));
    float field_cy = y + kPolkitFieldHeight * 0.5f;
    float dots_x0 = content_x + kPolkitDotMargin;
    float dots_w = content_w - kPolkitDotMargin * 2.0f;
    int dots_w_px = static_cast<int>(dots_w);
    if (needs_input) {
        int n = static_cast<int>(utf8_len(state.password.text));
        if (!state.password.error_message.empty()) {
            const Texture *et =
                tc_text(state, state.password.error_message, 12, false, scale, dots_w_px);
            if (et)
                node_add_texture_rect(card, content_cx - px_w(et) * 0.5f, field_cy - px_h(et) * 0.5f, px_w(et), px_h(et), *et, rgba(palette::critical));
        } else if (n == 0) {
            const Texture *pt =
                tc_text(state, "Password", 12, false, scale, dots_w_px);
            if (pt)
                node_add_texture_rect(card, content_cx - px_w(pt) * 0.5f, field_cy - px_h(pt) * 0.5f, px_w(pt), px_h(pt), *pt, rgba(palette::text_muted));
        } else {
            float row_w = static_cast<float>(n) * kPolkitDotSize;
            float dot_x = dots_x0 + (dots_w - row_w) * 0.5f;
            for (int i = 0; i < n; ++i) {
                const TextFieldCharAnim *anim =
                    i < static_cast<int>(state.pw_anim.chars.size())
                        ? &state.pw_anim.chars[static_cast<size_t>(i)]
                        : nullptr;
                float dsc = anim ? anim->scale : 1.0f;
                float dsz = kPolkitDotSize * dsc;
                float gx = dot_x + static_cast<float>(i) * kPolkitDotSize;
                float gy = field_cy - dsz * 0.5f;
                if (state.echo_glyph.id)
                    node_add_texture_rect(card, gx, gy, dsz, dsz, state.echo_glyph, rgba(palette::text));
                else
                    node_add_rrect(card, gx, gy, dsz, dsz, dsz * 0.5f, 0.0f, rgba(palette::text), kNodeTransparent);
            }
        }
    } else {
        const char *placeholder = "Authenticating...";
        const Texture *pt =
            tc_text(state, placeholder, 12, false, scale, dots_w_px);
        if (pt)
            node_add_texture_rect(card, content_cx - px_w(pt) * 0.5f, field_cy - px_h(pt) * 0.5f, px_w(pt), px_h(pt), *pt, rgba(palette::text_muted));
    }
    y += kPolkitFieldHeight;

    if (show_supplementary) {
        y += kPolkitSpacing;
        const Texture *sup_t =
            tc_text(state, supplementary, 11, false, scale, content_w_px);
        if (sup_t)
            node_add_texture_rect(card, content_x, y, px_w(sup_t), px_h(sup_t), *sup_t, rgba(palette::text_muted));
    }

    state.scene.draw(*state.renderer);
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive())
        overlay_panel_request_frame(state.base);
}
