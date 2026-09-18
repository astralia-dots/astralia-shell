#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <xkbcommon/xkbcommon-names.h>

#include "service/input_service.h"

std::optional<KeyEvent> translate_key(xkb_state *state, uint32_t keycode, xkb_compose_state *compose) {
    xkb_keycode_t xkb_code = keycode + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, xkb_code);
    bool shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
    bool alt = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0;
    bool ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0;

    if (compose) {
        xkb_compose_state_feed(compose, sym);
        xkb_compose_status status = xkb_compose_state_get_status(compose);
        if (status == XKB_COMPOSE_COMPOSED) {
            char buf[32];
            int n = xkb_compose_state_get_utf8(compose, buf, sizeof(buf));
            xkb_compose_state_reset(compose);
            if (n <= 0)
                return std::nullopt;
            return KeyEvent{KeyKind::Text,
                            std::string(buf, static_cast<size_t>(n))};
        }
        if (status == XKB_COMPOSE_CANCELLED) {
            xkb_compose_state_reset(compose);
            return std::nullopt;
        }
        if (status == XKB_COMPOSE_COMPOSING) {
            char buf[8];
            int n = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
            if (n <= 0) {
                char name[64];
                if (xkb_keysym_get_name(sym, name, sizeof(name)) > 0 && std::strncmp(name, "dead_", 5) == 0) {
                    xkb_keysym_t base =
                        xkb_keysym_from_name(name + 5, XKB_KEYSYM_NO_FLAGS);
                    if (base != XKB_KEY_NoSymbol)
                        n = xkb_keysym_to_utf8(base, buf, sizeof(buf));
                }
            }
            if (n <= 0)
                return std::nullopt;
            return KeyEvent{KeyKind::Preedit,
                            std::string(buf, static_cast<size_t>(n - 1))};
        }
    }

    switch (sym) {
    case XKB_KEY_Up:
        return KeyEvent{KeyKind::Up, "", shift, alt, ctrl};
    case XKB_KEY_Down:
        return KeyEvent{KeyKind::Down, "", shift, alt, ctrl};
    case XKB_KEY_Left:
        return KeyEvent{KeyKind::Left, "", shift, alt, ctrl};
    case XKB_KEY_Right:
        return KeyEvent{KeyKind::Right, "", shift, alt, ctrl};
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        return KeyEvent{KeyKind::Enter, ""};
    case XKB_KEY_Escape:
        return KeyEvent{KeyKind::Escape, ""};
    case XKB_KEY_BackSpace:
        return KeyEvent{KeyKind::Backspace, ""};
    case XKB_KEY_Tab:
        return KeyEvent{KeyKind::Tab, ""};
    default:
        break;
    }

    char buf[32];
    int n = xkb_state_key_get_utf8(state, xkb_code, buf, sizeof(buf));
    if (n <= 0)
        return std::nullopt;
    return KeyEvent{KeyKind::Text, std::string(buf, static_cast<size_t>(n)),
                    shift, alt, ctrl};
}

std::vector<KeyEvent> keyboard_drain_events(KeyboardState &state) {
    std::vector<KeyEvent> events = std::move(state.pending);
    state.pending.clear();
    return events;
}

void keyboard_repeat_tick(KeyboardState &state) {
    if (state.repeat_timer_fd < 0)
        return;
    uint64_t expirations = 0;
    ssize_t n = read(state.repeat_timer_fd, &expirations, sizeof(expirations));
    if (n != sizeof(expirations) || !state.repeat_active || !state.xkb)
        return;
    auto ev =
        translate_key(state.xkb, state.repeat_keycode, state.compose_state);
    if (!ev)
        return;
    constexpr uint64_t kMaxCatchUp = 8;
    for (uint64_t i = 0; i < std::min(expirations, kMaxCatchUp); ++i)
        state.pending.push_back(*ev);
}

std::vector<PointerClick> pointer_drain_clicks(PointerState &state) {
    std::vector<PointerClick> clicks = std::move(state.pending_clicks);
    state.pending_clicks.clear();
    return clicks;
}

std::vector<PointerScroll> pointer_drain_scrolls(PointerState &state) {
    std::vector<PointerScroll> scrolls = std::move(state.pending_scrolls);
    state.pending_scrolls.clear();
    return scrolls;
}
