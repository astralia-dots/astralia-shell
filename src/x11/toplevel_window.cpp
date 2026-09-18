#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstring>
#include <unordered_map>

#include "x11/atoms.h"
#include "x11/toplevel_window.h"

#include "app/backend.h"

#include "render/gl.h"

namespace backend_x11 {

namespace {

std::unordered_map<Window, ToplevelWindowBase *> &owner_table() {
    static std::unordered_map<Window, ToplevelWindowBase *> table;
    return table;
}

} // namespace

bool toplevel_window_create_surface(ToplevelWindowBase &base, void *, void *, const char *title, const char *app_id, int32_t default_width, int32_t default_height) {
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo))
        return false;

    XSetWindowAttributes attrs{};
    attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;
    attrs.event_mask = StructureNotifyMask;

    Window window = XCreateWindow(display, root, 0, 0, static_cast<unsigned>(default_width), static_cast<unsigned>(default_height), 0, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &attrs);
    base.surface = reinterpret_cast<void *>(static_cast<uintptr_t>(window));
    owner_table()[window] = &base;
    base.width = default_width;
    base.height = default_height;
    base.pending_width = default_width;
    base.pending_height = default_height;

    XStoreName(display, window, title);
    XChangeProperty(display, window, x11_atoms::net_wm_name(), x11_atoms::utf8_string(), 8, PropModeReplace, reinterpret_cast<const unsigned char *>(title), static_cast<int>(strlen(title)));

    XClassHint class_hint{const_cast<char *>(app_id), const_cast<char *>(app_id)};
    XSetClassHint(display, window, &class_hint);

    XSizeHints size_hints{};
    size_hints.flags = PSize;
    size_hints.width = default_width;
    size_hints.height = default_height;
    XSetWMNormalHints(display, window, &size_hints);

    Atom delete_window = x11_atoms::wm_delete_window();
    XSetWMProtocols(display, window, &delete_window, 1);

    XMapWindow(display, window);
    XFlush(display);

    base.configured = true;
    return true;
}

bool toplevel_window_init_egl(ToplevelWindowBase &base, EGLDisplay display, EGLConfig config, EGLContext context) {
    base.egl_display = display;
    base.egl_context = context;
    base.egl_window = egl_native_window_create(base.surface, base.width, base.height);
    base.egl_surface = egl_surface_create(base.surface, base.egl_window, display, config);
    if (base.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(display, base.egl_surface, context))
        return false;
    base.frame_clock.surface = base.surface;
    return true;
}

void toplevel_window_request_frame(ToplevelWindowBase &base) {
    if (base.egl_surface == EGL_NO_SURFACE)
        return;
    request_frame(base.frame_clock);
}

void toplevel_window_destroy_surface(ToplevelWindowBase &base) {
    if (base.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(base.egl_display, base.egl_surface);
        base.egl_surface = EGL_NO_SURFACE;
    }
    if (base.surface) {
        auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(base.surface));
        owner_table().erase(window);
        XDestroyWindow(static_cast<Display *>(active_display()), window);
        base.surface = nullptr;
    }
    frame_clock_drop_callback(base.frame_clock);
    base.frame_clock.redraw_requested = false;
    base.frame_clock.mapped = false;
    base.frame_clock.surface = nullptr;
    base.configured = false;
}

void toplevel_window_handle_configure_notify(Window window, int32_t width, int32_t height) {
    auto it = owner_table().find(window);
    if (it == owner_table().end())
        return;
    ToplevelWindowBase &base = *it->second;
    if (width == base.width && height == base.height)
        return;
    base.width = width;
    base.height = height;
    int32_t scale = base.output_scale.scale;
    if (base.egl_window)
        egl_native_window_resize(base.egl_window, width * scale, height * scale);
    if (base.frame_clock.surface)
        request_frame(base.frame_clock);
}

void toplevel_window_handle_client_message(Window window, Atom protocol_atom) {
    auto it = owner_table().find(window);
    if (it == owner_table().end())
        return;
    if (protocol_atom == x11_atoms::wm_delete_window() && it->second->on_close_request)
        it->second->on_close_request();
}

} // namespace backend_x11
