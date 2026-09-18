#include "app/backend_poll.h"

#include "app/backend.h"
#include "app/wayland_state.h"

#include "x11/input_service.h"
#include "x11/layer_surface.h"
#include "x11/toplevel_window.h"

#ifdef ASTRALIA_HAVE_X11
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#endif
#ifdef ASTRALIA_HAVE_WAYLAND
#include <wayland-client.h>
#endif

void backend_poll_flush(WaylandState &app) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        wl_display_flush(app.display);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    XFlush(static_cast<Display *>(active_display()));
#endif
}

int backend_poll_fd(WaylandState &app) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return wl_display_get_fd(app.display);
#endif
#ifdef ASTRALIA_HAVE_X11
    return ConnectionNumber(static_cast<Display *>(active_display()));
#else
    (void)app;
    return -1;
#endif
}

void backend_poll_dispatch(WaylandState &app) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        wl_display_dispatch(app.display);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    auto *display = static_cast<Display *>(active_display());
    int xi_op = backend_x11::xi_opcode();
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == GenericEvent && event.xcookie.extension == xi_op) {
            if (XGetEventData(display, &event.xcookie)) {
                auto *device_event = static_cast<XIDeviceEvent *>(event.xcookie.data);
                backend_x11::handle_xi_device_event(app.seat_caps, event.xcookie.evtype, *device_event);
                XFreeEventData(display, &event.xcookie);
            }
        } else if (event.type == ConfigureNotify) {
            backend_x11::toplevel_window_handle_configure_notify(event.xconfigure.window, event.xconfigure.width, event.xconfigure.height);
            backend_x11::layer_surface_handle_configure_notify(event.xconfigure.window, event.xconfigure.x, event.xconfigure.y, event.xconfigure.width, event.xconfigure.height);
        } else if (event.type == ClientMessage) {
            backend_x11::toplevel_window_handle_client_message(event.xclient.window, static_cast<Atom>(event.xclient.data.l[0]));
        }
    }
#else
    (void)app;
#endif
}
