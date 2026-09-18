#ifdef ASTRALIA_HAVE_X11
#include <X11/Xlib.h>
#endif
#ifdef ASTRALIA_HAVE_WAYLAND
#include <wayland-client.h>
#endif

#include "app/backend.h"

namespace {
Backend g_active_backend = Backend::Wayland;
void *g_active_display = nullptr;
} // namespace

BackendConnection backend_connect() {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (wl_display *display = wl_display_connect(nullptr)) {
        g_active_backend = Backend::Wayland;
        g_active_display = display;
        return {Backend::Wayland, display};
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    if (Display *display = XOpenDisplay(nullptr)) {
        g_active_backend = Backend::X11;
        g_active_display = display;
        return {Backend::X11, display};
    }
#endif
    return {Backend::Wayland, nullptr};
}

Backend active_backend() { return g_active_backend; }

void *active_display() { return g_active_display; }

void backend_wait_dispatch() {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (g_active_backend == Backend::Wayland) {
        wl_display_dispatch(static_cast<wl_display *>(g_active_display));
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    auto *display = static_cast<Display *>(g_active_display);
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
    }
#endif
}
