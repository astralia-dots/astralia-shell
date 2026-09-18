#include "service/frame_service.h"

#include "app/backend.h"

#include "wayland/frame_service.h"

#include "x11/frame_service.h"

void request_frame(FrameClock &clock) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::request_frame(clock);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::request_frame(clock);
#endif
}

void frame_clock_drop_callback(FrameClock &clock) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::frame_clock_drop_callback(clock);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::frame_clock_drop_callback(clock);
#endif
}
