#include "x11/atoms.h"

#include "app/backend.h"

namespace x11_atoms {

namespace {

Atom cached(Atom &slot, const char *name) {
    if (slot == None)
        slot = XInternAtom(static_cast<Display *>(active_display()), name, False);
    return slot;
}

} // namespace

Atom net_wm_strut() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_STRUT");
}

Atom net_wm_strut_partial() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_STRUT_PARTIAL");
}

Atom net_wm_desktop() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_DESKTOP");
}

Atom net_wm_window_type() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_WINDOW_TYPE");
}

Atom net_wm_window_type_dock() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_WINDOW_TYPE_DOCK");
}

Atom net_wm_state() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_STATE");
}

Atom net_wm_state_above() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_STATE_ABOVE");
}

Atom net_wm_state_below() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_STATE_BELOW");
}

Atom net_wm_name() {
    static Atom slot = None;
    return cached(slot, "_NET_WM_NAME");
}

Atom utf8_string() {
    static Atom slot = None;
    return cached(slot, "UTF8_STRING");
}

Atom wm_protocols() {
    static Atom slot = None;
    return cached(slot, "WM_PROTOCOLS");
}

Atom wm_delete_window() {
    static Atom slot = None;
    return cached(slot, "WM_DELETE_WINDOW");
}

} // namespace x11_atoms
