#include <clocale>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wayland/input_service.h"

#include "core/log.h"

#include "cursor-shape-v1-client-protocol.h"

namespace backend_wayland {

namespace {

namespace kbd {

void keymap_cb(void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size) {
    auto *state = static_cast<KeyboardState *>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED)
        return;

    if (state->keymap)
        xkb_keymap_unref(state->keymap);
    if (state->xkb)
        xkb_state_unref(state->xkb);
    state->xkb = nullptr;

    state->keymap = xkb_keymap_new_from_string(state->ctx, static_cast<const char *>(map), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    if (!state->keymap) {
        klog("keyboard: failed to compile keymap");
        return;
    }
    state->xkb = xkb_state_new(state->keymap);
}

void enter_cb(void *data, wl_keyboard *, uint32_t, wl_surface *surface, wl_array *) {
    auto *state = static_cast<KeyboardState *>(data);
    state->focused_surface = surface;
    if (state->on_focus_surface)
        state->on_focus_surface(surface, true);
}

void set_repeat_timer(KeyboardState &state, bool armed) {
    if (state.repeat_timer_fd < 0)
        return;
    itimerspec spec{};
    if (armed && state.repeat_rate_hz > 0) {
        spec.it_value.tv_sec = state.repeat_delay_ms / 1000;
        spec.it_value.tv_nsec = (state.repeat_delay_ms % 1000) * 1000000L;
        int64_t interval_ns = 1000000000LL / state.repeat_rate_hz;
        spec.it_interval.tv_sec = interval_ns / 1000000000LL;
        spec.it_interval.tv_nsec = interval_ns % 1000000000LL;
    }

    timerfd_settime(state.repeat_timer_fd, 0, &spec, nullptr);
}

void leave_cb(void *data, wl_keyboard *, uint32_t, wl_surface *surface) {
    auto *state = static_cast<KeyboardState *>(data);
    if (state->repeat_active) {
        state->repeat_active = false;
        set_repeat_timer(*state, false);
    }
    if (state->focused_surface == surface)
        state->focused_surface = nullptr;
    if (state->on_focus_surface)
        state->on_focus_surface(surface, false);
}

void key_cb(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t key_state) {
    auto *state = static_cast<KeyboardState *>(data);
    if (!state->xkb)
        return;
    xkb_keycode_t xkb_code = key + 8;

    if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (state->repeat_active && key == state->repeat_keycode) {
            state->repeat_active = false;
            set_repeat_timer(*state, false);
        }
        return;
    }
    if (key_state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;

    if (auto ev = translate_key(state->xkb, key, state->compose_state))
        state->pending.push_back(*ev);

    if (state->keymap && xkb_keymap_key_repeats(state->keymap, xkb_code)) {
        state->repeat_keycode = key;
        state->repeat_active = true;
        set_repeat_timer(*state, true);
    }
}

void modifiers_cb(void *data, wl_keyboard *, uint32_t, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    auto *state = static_cast<KeyboardState *>(data);
    if (!state->xkb)
        return;
    xkb_state_update_mask(state->xkb, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void repeat_info_cb(void *data, wl_keyboard *, int32_t rate, int32_t delay) {
    auto *state = static_cast<KeyboardState *>(data);
    klog("keyboard: compositor repeat_info rate=%dHz delay=%dms", rate, delay);
    state->repeat_rate_hz = rate;
    state->repeat_delay_ms = delay;

    if (state->repeat_active)
        set_repeat_timer(*state, true);
}

constexpr wl_keyboard_listener kKeyboardListener = {
    .keymap = keymap_cb,
    .enter = enter_cb,
    .leave = leave_cb,
    .key = key_cb,
    .modifiers = modifiers_cb,
    .repeat_info = repeat_info_cb,
};

} // namespace kbd

namespace ptr {

wp_cursor_shape_device_v1_shape to_wp_shape(PointerShape shape) {
    switch (shape) {
    case PointerShape::Pointer:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
    case PointerShape::Default:
    default:
        return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    }
}

void set_cursor_shape(PointerState &state, wp_cursor_shape_device_v1_shape shape) {
    auto *manager = static_cast<wp_cursor_shape_manager_v1 *>(state.cursor_shape_manager);
    auto *pointer = static_cast<wl_pointer *>(state.pointer);
    if (!manager || !pointer)
        return;
    if (!state.cursor_shape_device)
        state.cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(manager, pointer);
    wp_cursor_shape_device_v1_set_shape(static_cast<wp_cursor_shape_device_v1 *>(state.cursor_shape_device), state.last_enter_serial, shape);
}

void enter_cb(void *data, wl_pointer *, uint32_t serial, wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy) {
    auto *state = static_cast<PointerState *>(data);
    state->focused_surface = surface;
    state->x = wl_fixed_to_double(sx);
    state->y = wl_fixed_to_double(sy);
    state->dirty = true;
    state->last_enter_serial = serial;
    set_cursor_shape(*state, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
}

void leave_cb(void *data, wl_pointer *, uint32_t, wl_surface *surface) {
    auto *state = static_cast<PointerState *>(data);
    if (state->focused_surface == surface) {
        state->focused_surface = nullptr;
        state->dirty = true;
    }
}

void motion_cb(void *data, wl_pointer *, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    auto *state = static_cast<PointerState *>(data);
    state->x = wl_fixed_to_double(sx);
    state->y = wl_fixed_to_double(sy);
    state->dirty = true;
}

void button_cb(void *data, wl_pointer *, uint32_t serial, uint32_t, uint32_t button, uint32_t button_state) {
    auto *state = static_cast<PointerState *>(data);
    if (button != BTN_LEFT && button != BTN_RIGHT)
        return;
    if (button_state == WL_POINTER_BUTTON_STATE_PRESSED)
        state->last_button_serial = serial;
    state->pending_clicks.push_back({state->focused_surface, button_state == WL_POINTER_BUTTON_STATE_PRESSED, button, state->x, state->y, serial});
}
void axis_cb(void *data, wl_pointer *, uint32_t, uint32_t axis, wl_fixed_t value) {
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
        return;
    auto *state = static_cast<PointerState *>(data);
    state->pending_scrolls.push_back({state->focused_surface, wl_fixed_to_double(value)});
}
void frame_cb(void *, wl_pointer *) {}
void axis_source_cb(void *, wl_pointer *, uint32_t) {}
void axis_stop_cb(void *, wl_pointer *, uint32_t, uint32_t) {}
void axis_discrete_cb(void *, wl_pointer *, uint32_t, int32_t) {}
void axis_value120_cb(void *, wl_pointer *, uint32_t, int32_t) {}
void axis_relative_direction_cb(void *, wl_pointer *, uint32_t, uint32_t) {}
void warp_cb(void *, wl_pointer *, wl_fixed_t, wl_fixed_t) {}

constexpr wl_pointer_listener kPointerListener = {
    .enter = enter_cb,
    .leave = leave_cb,
    .motion = motion_cb,
    .button = button_cb,
    .axis = axis_cb,
    .frame = frame_cb,
    .axis_source = axis_source_cb,
    .axis_stop = axis_stop_cb,
    .axis_discrete = axis_discrete_cb,
    .axis_value120 = axis_value120_cb,
    .axis_relative_direction = axis_relative_direction_cb,
    .warp = warp_cb,
};

} // namespace ptr

void seat_capabilities_cb(void *data, wl_seat *seat, uint32_t caps) {
    auto *seat_state = static_cast<SeatCapabilityState *>(data);

    KeyboardState *kb = seat_state->keyboard;
    bool has_keyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;
    if (has_keyboard && !kb->keyboard) {
        auto *keyboard = wl_seat_get_keyboard(seat);
        kb->keyboard = keyboard;
        wl_keyboard_add_listener(keyboard, &kbd::kKeyboardListener, kb);
    } else if (!has_keyboard && kb->keyboard) {
        wl_keyboard_release(static_cast<wl_keyboard *>(kb->keyboard));
        kb->keyboard = nullptr;
    }

    PointerState *pointer = seat_state->pointer;
    bool has_pointer = caps & WL_SEAT_CAPABILITY_POINTER;
    if (has_pointer && !pointer->pointer) {
        backend_wayland::pointer_bind(*pointer, seat);
    } else if (!has_pointer && pointer->pointer) {
        backend_wayland::pointer_release(*pointer);
    }
}

void seat_name_cb(void *, wl_seat *, const char *) {}

constexpr wl_seat_listener kSeatListener = {
    .capabilities = seat_capabilities_cb,
    .name = seat_name_cb,
};

} // namespace

void keyboard_attach_seat(SeatCapabilityState &seat_state, void *seat_v) {
    auto *seat = static_cast<wl_seat *>(seat_v);
    if (!seat_state.keyboard->ctx)
        seat_state.keyboard->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!seat_state.keyboard->compose_table) {
        const char *locale = setlocale(LC_CTYPE, "");
        seat_state.keyboard->compose_table = xkb_compose_table_new_from_locale(seat_state.keyboard->ctx, locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (seat_state.keyboard->compose_table)
            seat_state.keyboard->compose_state = xkb_compose_state_new(seat_state.keyboard->compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
    }
    if (seat_state.keyboard->repeat_timer_fd < 0) {
        seat_state.keyboard->repeat_timer_fd =
            timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    }
    wl_seat_add_listener(seat, &kSeatListener, &seat_state);
}

void pointer_bind(PointerState &state, void *seat_v) {
    auto *seat = static_cast<wl_seat *>(seat_v);
    auto *pointer = wl_seat_get_pointer(seat);
    state.pointer = pointer;
    wl_pointer_add_listener(pointer, &ptr::kPointerListener, &state);
}

void pointer_release(PointerState &state) {
    if (state.cursor_shape_device) {
        wp_cursor_shape_device_v1_destroy(static_cast<wp_cursor_shape_device_v1 *>(state.cursor_shape_device));
        state.cursor_shape_device = nullptr;
    }
    if (state.pointer) {
        wl_pointer_release(static_cast<wl_pointer *>(state.pointer));
        state.pointer = nullptr;
    }
    state.focused_surface = nullptr;
}

void pointer_set_cursor_shape(PointerState &state, PointerShape shape) {
    ptr::set_cursor_shape(state, ptr::to_wp_shape(shape));
}

} // namespace backend_wayland
