#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <clocale>
#include <cstdint>
#include <sys/timerfd.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "x11/input_service.h"

#include "app/backend.h"

#include "core/log.h"

namespace backend_x11 {

namespace {

constexpr int kButtonLeft = 1;
constexpr int kButtonRight = 3;
constexpr int kButtonScrollUp = 4;
constexpr int kButtonScrollDown = 5;
constexpr double kScrollTick = 15.0;

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

int query_xi_opcode(Display *display) {
    int opcode = 0, event = 0, error = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &error))
        return -1;
    int major = 2, minor = 0;
    if (XIQueryVersion(display, &major, &minor) != Success)
        return -1;
    return opcode;
}

} // namespace

int xi_opcode() {
    static int opcode = query_xi_opcode(static_cast<Display *>(active_display()));
    return opcode;
}

void keyboard_attach_seat(SeatCapabilityState &seat_state, void *) {
    auto *display = static_cast<Display *>(active_display());
    KeyboardState &kb = *seat_state.keyboard;

    if (!kb.ctx)
        kb.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!kb.compose_table) {
        const char *locale = setlocale(LC_CTYPE, "");
        kb.compose_table = xkb_compose_table_new_from_locale(kb.ctx, locale ? locale : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (kb.compose_table)
            kb.compose_state = xkb_compose_state_new(kb.compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
    }
    if (kb.repeat_timer_fd < 0)
        kb.repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    xcb_connection_t *xcb_conn = XGetXCBConnection(display);
    int32_t core_kbd_id = xkb_x11_get_core_keyboard_device_id(xcb_conn);
    if (core_kbd_id < 0) {
        klog("keyboard: xkb_x11_get_core_keyboard_device_id failed");
        return;
    }
    if (kb.keymap)
        xkb_keymap_unref(kb.keymap);
    if (kb.xkb)
        xkb_state_unref(kb.xkb);
    kb.keymap = xkb_x11_keymap_new_from_device(kb.ctx, xcb_conn, core_kbd_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!kb.keymap) {
        klog("keyboard: xkb_x11_keymap_new_from_device failed");
        return;
    }
    kb.xkb = xkb_x11_state_new_from_device(kb.keymap, xcb_conn, core_kbd_id);

    if (query_xi_opcode(display) < 0) {
        klog("keyboard: XInput2 unavailable");
        return;
    }
    XIEventMask mask;
    unsigned char mask_bits[XIMaskLen(XI_LASTEVENT)] = {0};
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = sizeof(mask_bits);
    mask.mask = mask_bits;
    XISetMask(mask_bits, XI_KeyPress);
    XISetMask(mask_bits, XI_KeyRelease);
    XISetMask(mask_bits, XI_FocusIn);
    XISetMask(mask_bits, XI_FocusOut);
    XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
}

void pointer_bind(PointerState &, void *) {
    auto *display = static_cast<Display *>(active_display());
    XIEventMask mask;
    unsigned char mask_bits[XIMaskLen(XI_LASTEVENT)] = {0};
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = sizeof(mask_bits);
    mask.mask = mask_bits;
    XISetMask(mask_bits, XI_ButtonPress);
    XISetMask(mask_bits, XI_ButtonRelease);
    XISetMask(mask_bits, XI_Motion);
    XISetMask(mask_bits, XI_Enter);
    XISetMask(mask_bits, XI_Leave);
    XISelectEvents(display, DefaultRootWindow(display), &mask, 1);
}

void pointer_release(PointerState &state) {
    state.focused_surface = nullptr;
}

void pointer_set_cursor_shape(PointerState &state, PointerShape shape) {
    if (!state.focused_surface)
        return;
    auto *display = static_cast<Display *>(active_display());
    auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(state.focused_surface));
    const char *name = shape == PointerShape::Pointer ? "pointer" : "default";
    Cursor cursor = XcursorLibraryLoadCursor(display, name);
    if (cursor != None)
        XDefineCursor(display, window, cursor);
}

void handle_xi_device_event(SeatCapabilityState &seat_state, int xi_event_type, const XIDeviceEvent &event) {
    KeyboardState &kb = *seat_state.keyboard;
    PointerState &ptr = *seat_state.pointer;
    Window target = event.child != None ? event.child : event.event;
    auto surface = reinterpret_cast<NativeSurfaceHandle>(static_cast<uintptr_t>(target));

    switch (xi_event_type) {
    case XI_FocusIn:
        kb.focused_surface = surface;
        if (kb.on_focus_surface)
            kb.on_focus_surface(surface, true);
        return;
    case XI_FocusOut:
        if (kb.repeat_active) {
            kb.repeat_active = false;
            set_repeat_timer(kb, false);
        }
        if (kb.focused_surface == surface)
            kb.focused_surface = nullptr;
        if (kb.on_focus_surface)
            kb.on_focus_surface(surface, false);
        return;
    case XI_KeyPress: {
        if (!kb.xkb)
            return;
        xkb_state_update_mask(kb.xkb, event.mods.base, event.mods.latched, event.mods.locked, event.group.base, event.group.latched, event.group.locked);
        uint32_t evdev_keycode = static_cast<uint32_t>(event.detail) - 8;
        xkb_keycode_t xkb_code = static_cast<xkb_keycode_t>(event.detail);
        if (auto ev = translate_key(kb.xkb, evdev_keycode, kb.compose_state))
            kb.pending.push_back(*ev);
        if (kb.keymap && xkb_keymap_key_repeats(kb.keymap, xkb_code)) {
            kb.repeat_keycode = evdev_keycode;
            kb.repeat_active = true;
            set_repeat_timer(kb, true);
        }
        return;
    }
    case XI_KeyRelease:
        if (kb.xkb)
            xkb_state_update_mask(kb.xkb, event.mods.base, event.mods.latched, event.mods.locked, event.group.base, event.group.latched, event.group.locked);
        if (kb.repeat_active && static_cast<uint32_t>(event.detail) - 8 == kb.repeat_keycode) {
            kb.repeat_active = false;
            set_repeat_timer(kb, false);
        }
        return;
    case XI_Enter:
        ptr.focused_surface = surface;
        ptr.x = event.event_x;
        ptr.y = event.event_y;
        ptr.dirty = true;
        backend_x11::pointer_set_cursor_shape(ptr, PointerShape::Default);
        return;
    case XI_Leave:
        if (ptr.focused_surface == surface) {
            ptr.focused_surface = nullptr;
            ptr.dirty = true;
        }
        return;
    case XI_Motion:
        ptr.x = event.event_x;
        ptr.y = event.event_y;
        ptr.dirty = true;
        return;
    case XI_ButtonPress:
    case XI_ButtonRelease: {
        bool pressed = xi_event_type == XI_ButtonPress;
        if (event.detail == kButtonScrollUp || event.detail == kButtonScrollDown) {
            if (pressed)
                ptr.pending_scrolls.push_back({surface, event.detail == kButtonScrollDown ? kScrollTick : -kScrollTick});
            return;
        }
        if (event.detail != kButtonLeft && event.detail != kButtonRight)
            return;
        uint32_t button = event.detail == kButtonLeft ? BTN_LEFT : BTN_RIGHT;
        ptr.pending_clicks.push_back({surface, pressed, button, event.event_x, event.event_y, static_cast<uint32_t>(event.serial)});
        return;
    }
    default:
        return;
    }
}

} // namespace backend_x11
