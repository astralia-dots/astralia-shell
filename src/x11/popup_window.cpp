#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <algorithm>

#include "x11/popup_window.h"

#include "app/backend.h"

#include "render/gl.h"

namespace backend_x11 {

namespace {

void compute_position(Rect anchor_rect, int32_t w, int32_t h, int32_t &x, int32_t &y) {
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    int32_t screen_w = DisplayWidth(display, screen);
    int32_t screen_h = DisplayHeight(display, screen);

    x = static_cast<int32_t>(anchor_rect.x);
    y = static_cast<int32_t>(anchor_rect.y + anchor_rect.h);
    x = std::clamp(x, 0, std::max(0, screen_w - w));
    y = std::clamp(y, 0, std::max(0, screen_h - h));
}

} // namespace

bool popup_window_create(PopupWindowBase &base, void *, void *, void *, Rect anchor_rect, int32_t w, int32_t h, void *, uint32_t) {
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo))
        return false;

    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;

    int32_t x, y;
    compute_position(anchor_rect, w, h, x, y);

    Window window = XCreateWindow(display, root, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h), 0, vinfo.depth, InputOutput, vinfo.visual, CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel, &attrs);
    base.surface = reinterpret_cast<void *>(static_cast<uintptr_t>(window));
    base.width = w;
    base.height = h;
    base.done = false;
    base.configured = true;

    XMapWindow(display, window);
    XFlush(display);
    return true;
}

bool popup_window_init_egl(PopupWindowBase &base, void *, EGLDisplay egl_display, EGLConfig config, EGLContext context) {
    base.egl_display = egl_display;
    base.egl_context = context;
    base.egl_window = egl_native_window_create(base.surface, base.width, base.height);
    base.egl_surface = egl_surface_create(base.surface, base.egl_window, egl_display, config);
    if (base.egl_surface == EGL_NO_SURFACE)
        return false;
    if (!gl_make_current(egl_display, base.egl_surface, context))
        return false;
    base.frame_clock.surface = base.surface;
    return true;
}

void popup_window_reposition(PopupWindowBase &base, void *, Rect anchor_rect, int32_t w, int32_t h) {
    if (!base.surface)
        return;
    int32_t x, y;
    compute_position(anchor_rect, w, h, x, y);
    auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(base.surface));
    XMoveResizeWindow(static_cast<Display *>(active_display()), window, x, y, static_cast<unsigned>(w), static_cast<unsigned>(h));
    base.width = w;
    base.height = h;
    base.configured = true;
}

void popup_window_request_frame(PopupWindowBase &base) {
    if (base.egl_surface == EGL_NO_SURFACE || base.done)
        return;
    request_frame(base.frame_clock);
}

void popup_window_destroy(PopupWindowBase &base) {
    frame_clock_drop_callback(base.frame_clock);
    base.frame_clock.surface = nullptr;
    base.frame_clock.redraw_requested = false;
    base.frame_clock.mapped = false;

    if (base.egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(base.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(base.egl_display, base.egl_surface);
        base.egl_surface = EGL_NO_SURFACE;
    }
    if (base.surface) {
        XDestroyWindow(static_cast<Display *>(active_display()), static_cast<Window>(reinterpret_cast<uintptr_t>(base.surface)));
        base.surface = nullptr;
    }
    base.configured = false;
    base.done = false;
}

} // namespace backend_x11
