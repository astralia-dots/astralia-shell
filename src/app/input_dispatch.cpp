#include "app/backend.h"

#include "service/input_service.h"

#include "wayland/input_service.h"

#include "x11/input_service.h"

void keyboard_attach_seat(SeatCapabilityState &seat_state, void *seat) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::keyboard_attach_seat(seat_state, seat);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::keyboard_attach_seat(seat_state, seat);
#endif
}

void pointer_bind(PointerState &state, void *seat) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::pointer_bind(state, seat);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::pointer_bind(state, seat);
#endif
}

void pointer_release(PointerState &state) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::pointer_release(state);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::pointer_release(state);
#endif
}

void pointer_set_cursor_shape(PointerState &state, PointerShape shape) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::pointer_set_cursor_shape(state, shape);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::pointer_set_cursor_shape(state, shape);
#endif
}
