#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <array>
#include <cstdint>
#include <unordered_map>

#include "x11/atoms.h"
#include "x11/layer_surface.h"
#include "x11/output_bootstrap.h"

#include "app/backend.h"

namespace backend_x11 {

namespace {

constexpr uint32_t kAnchorTop = 1, kAnchorBottom = 2, kAnchorLeft = 4, kAnchorRight = 8;

struct X11LayerSurface {
    Window window = None;
    uint32_t anchor = 0;
    int32_t width = 0, height = 0;
    int32_t applied_x = 0, applied_y = 0;
    int32_t margin_top = 0, margin_right = 0, margin_bottom = 0, margin_left = 0;
    int32_t exclusive_zone = -1;
    MonitorRect monitor;
};

std::unordered_map<Window, X11LayerSurface *> &owner_table() {
    static std::unordered_map<Window, X11LayerSurface *> table;
    return table;
}

void apply_geometry(X11LayerSurface &ls) {
    auto *display = static_cast<Display *>(active_display());
    int32_t screen_w = ls.monitor.width;
    int32_t screen_h = ls.monitor.height;

    bool horiz = (ls.anchor & kAnchorLeft) && (ls.anchor & kAnchorRight);
    bool vert = (ls.anchor & kAnchorTop) && (ls.anchor & kAnchorBottom);

    int32_t x, y, w, h;
    if (horiz) {
        x = ls.margin_left;
        w = screen_w - ls.margin_left - ls.margin_right;
    } else if (ls.anchor & kAnchorLeft) {
        x = ls.margin_left;
        w = ls.width;
    } else if (ls.anchor & kAnchorRight) {
        w = ls.width;
        x = screen_w - w - ls.margin_right;
    } else {
        w = ls.width;
        x = (screen_w - w) / 2;
    }

    if (vert) {
        y = ls.margin_top;
        h = screen_h - ls.margin_top - ls.margin_bottom;
    } else if (ls.anchor & kAnchorTop) {
        y = ls.margin_top;
        h = ls.height;
    } else if (ls.anchor & kAnchorBottom) {
        h = ls.height;
        y = screen_h - h - ls.margin_bottom;
    } else {
        h = ls.height;
        y = (screen_h - h) / 2;
    }

    ls.applied_x = ls.monitor.x + x;
    ls.applied_y = ls.monitor.y + y;
    if (w > 0 && h > 0)
        XMoveResizeWindow(display, ls.window, ls.applied_x, ls.applied_y, static_cast<unsigned>(w), static_cast<unsigned>(h));
    ls.width = w;
    ls.height = h;
}

void apply_struts(X11LayerSurface &ls) {
    auto *display = static_cast<Display *>(active_display());

    bool has_h_edge = static_cast<bool>(ls.anchor & kAnchorLeft) != static_cast<bool>(ls.anchor & kAnchorRight);
    bool has_v_edge = static_cast<bool>(ls.anchor & kAnchorTop) != static_cast<bool>(ls.anchor & kAnchorBottom);

    if (ls.exclusive_zone <= 0 || !(has_h_edge != has_v_edge)) {
        XDeleteProperty(display, ls.window, x11_atoms::net_wm_strut());
        XDeleteProperty(display, ls.window, x11_atoms::net_wm_strut_partial());
        return;
    }

    int screen = DefaultScreen(display);
    int32_t root_w = DisplayWidth(display, screen);
    int32_t root_h = DisplayHeight(display, screen);

    std::array<int32_t, 12> data{};
    if (has_v_edge && (ls.anchor & kAnchorTop)) {
        data[2] = ls.monitor.y + ls.exclusive_zone;
        data[8] = ls.monitor.x;
        data[9] = ls.monitor.x + ls.width;
    } else if (has_v_edge) {
        data[3] = (root_h - (ls.monitor.y + ls.monitor.height)) + ls.exclusive_zone;
        data[10] = ls.monitor.x;
        data[11] = ls.monitor.x + ls.width;
    } else if (ls.anchor & kAnchorLeft) {
        data[0] = ls.monitor.x + ls.exclusive_zone;
        data[4] = ls.monitor.y;
        data[5] = ls.monitor.y + ls.height;
    } else {
        data[1] = (root_w - (ls.monitor.x + ls.monitor.width)) + ls.exclusive_zone;
        data[6] = ls.monitor.y;
        data[7] = ls.monitor.y + ls.height;
    }

    XChangeProperty(display, ls.window, x11_atoms::net_wm_strut(), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char *>(data.data()), 4);
    XChangeProperty(display, ls.window, x11_atoms::net_wm_strut_partial(), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char *>(data.data()), 12);
}

} // namespace

LayerSurfaceHandle layer_surface_create(NativeSurfaceHandle &out_surface, void *, void *, const LayerSurfaceConfig &cfg, LayerSurfaceConfigureFn on_configure, void *listener_data, void *output) {
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo))
        return nullptr;

    XSetWindowAttributes attrs{};
    attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
    attrs.border_pixel = 0;
    attrs.background_pixel = 0;
    attrs.event_mask = StructureNotifyMask;

    auto *ls = new X11LayerSurface{};
    ls->monitor = monitor_rect(output);
    ls->anchor = cfg.anchor;
    ls->width = cfg.width > 0 ? cfg.width : 1;
    ls->height = cfg.height > 0 ? cfg.height : 1;
    ls->margin_top = cfg.margin_top;
    ls->margin_right = cfg.margin_right;
    ls->margin_bottom = cfg.margin_bottom;
    ls->margin_left = cfg.margin_left;
    ls->exclusive_zone = cfg.exclusive_zone;

    ls->window = XCreateWindow(display, root, 0, 0, static_cast<unsigned>(ls->width), static_cast<unsigned>(ls->height), 0, vinfo.depth, InputOutput, vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &attrs);
    owner_table()[ls->window] = ls;

    Atom window_type = x11_atoms::net_wm_window_type_dock();
    XChangeProperty(display, ls->window, x11_atoms::net_wm_window_type(), XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char *>(&window_type), 1);

    uint32_t all_desktops = 0xffffffff;
    XChangeProperty(display, ls->window, x11_atoms::net_wm_desktop(), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char *>(&all_desktops), 1);

    Atom state = (cfg.layer <= kLayerShellBottom) ? x11_atoms::net_wm_state_below() : x11_atoms::net_wm_state_above();
    XChangeProperty(display, ls->window, x11_atoms::net_wm_state(), XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char *>(&state), 1);

    apply_geometry(*ls);
    apply_struts(*ls);

    XSizeHints size_hints{};
    size_hints.flags = PPosition | PSize;
    size_hints.x = ls->applied_x;
    size_hints.y = ls->applied_y;
    size_hints.width = ls->width;
    size_hints.height = ls->height;
    XSetWMNormalHints(display, ls->window, &size_hints);

    XMapWindow(display, ls->window);
    XFlush(display);

    out_surface = reinterpret_cast<void *>(static_cast<uintptr_t>(ls->window));

    if (on_configure)
        on_configure(listener_data, ls->width, ls->height);

    return ls;
}

void layer_surface_set_size(LayerSurfaceHandle layer_surface, int32_t width, int32_t height) {
    auto *ls = static_cast<X11LayerSurface *>(layer_surface);
    if (width > 0)
        ls->width = width;
    if (height > 0)
        ls->height = height;
    apply_geometry(*ls);
    apply_struts(*ls);
}

void layer_surface_set_margin(LayerSurfaceHandle layer_surface, int32_t top, int32_t right, int32_t bottom, int32_t left) {
    auto *ls = static_cast<X11LayerSurface *>(layer_surface);
    ls->margin_top = top;
    ls->margin_right = right;
    ls->margin_bottom = bottom;
    ls->margin_left = left;
    apply_geometry(*ls);
    apply_struts(*ls);
}

void layer_surface_set_exclusive_zone(LayerSurfaceHandle layer_surface, int32_t zone) {
    auto *ls = static_cast<X11LayerSurface *>(layer_surface);
    ls->exclusive_zone = zone;
    apply_struts(*ls);
}

void layer_surface_set_keyboard_interactivity(LayerSurfaceHandle layer_surface, bool exclusive) {
    auto *ls = static_cast<X11LayerSurface *>(layer_surface);
    auto *display = static_cast<Display *>(active_display());
    if (exclusive)
        XSetInputFocus(display, ls->window, RevertToParent, CurrentTime);
    else
        XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void layer_surface_handle_configure_notify(Window window, int32_t x, int32_t y, int32_t width, int32_t height) {
    auto it = owner_table().find(window);
    if (it == owner_table().end())
        return;
    X11LayerSurface &ls = *it->second;
    if (x == ls.applied_x && y == ls.applied_y && width == ls.width && height == ls.height)
        return;
    apply_geometry(ls);
    apply_struts(ls);
}

void destroy_layer_surface(EGLDisplay display, NativeSurfaceHandle &surface, LayerSurfaceHandle &layer_surface, NativeEglWindowHandle &egl_window, EGLSurface &egl_surface, FrameClock *frame_clock) {
    if (frame_clock)
        frame_clock_drop_callback(*frame_clock);
    if (egl_surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, egl_surface);
        egl_surface = EGL_NO_SURFACE;
    }
    if (layer_surface) {
        auto *ls = static_cast<X11LayerSurface *>(layer_surface);
        owner_table().erase(ls->window);
        XDestroyWindow(static_cast<Display *>(active_display()), ls->window);
        delete ls;
        layer_surface = nullptr;
    }
    surface = nullptr;
    (void)egl_window;
}

} // namespace backend_x11
