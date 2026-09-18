#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <cstdlib>
#include <vector>

#include "x11/capture_service.h"

#include "app/backend.h"

#include "core/log.h"

namespace backend_x11 {

namespace {

void swizzle_bgra_to_rgba(const uint8_t *src, uint32_t width, uint32_t height, std::vector<uint8_t> &dst) {
    dst.resize(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
}

void release_pixmap(ToplevelExportCapture &cap, Display *display) {
    if (cap.x11_pixmap) {
        XFreePixmap(display, static_cast<Pixmap>(reinterpret_cast<uintptr_t>(cap.x11_pixmap)));
        cap.x11_pixmap = nullptr;
    }
}

} // namespace

void toplevel_export_request(ToplevelExportState &state, void *, void *, const std::string &address, int min_interval_ms) {
    ToplevelExportCapture &cap = state.captures[address];
    if (cap.in_flight)
        return;
    if (cap.last_capture.time_since_epoch().count() != 0) {
        auto elapsed = std::chrono::steady_clock::now() - cap.last_capture;
        if (elapsed < std::chrono::milliseconds(min_interval_ms))
            return;
    }

    auto *display = static_cast<Display *>(active_display());
    auto window = static_cast<Window>(std::strtoull(address.c_str(), nullptr, 10));
    if (!window)
        return;

    cap.in_flight = true;

    XWindowAttributes attrs{};
    if (!XGetWindowAttributes(display, window, &attrs) || attrs.width <= 0 || attrs.height <= 0) {
        cap.in_flight = false;
        return;
    }

    XCompositeRedirectWindow(display, window, CompositeRedirectAutomatic);

    release_pixmap(cap, display);
    Pixmap pixmap = XCompositeNameWindowPixmap(display, window);
    if (!pixmap) {
        cap.in_flight = false;
        return;
    }
    cap.x11_pixmap = reinterpret_cast<void *>(static_cast<uintptr_t>(pixmap));

    auto width = static_cast<uint32_t>(attrs.width);
    auto height = static_cast<uint32_t>(attrs.height);
    XImage *img = XGetImage(display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!img) {
        cap.in_flight = false;
        return;
    }

    bool tight = static_cast<uint32_t>(img->bytes_per_line) == width * 4;
    int stride_px = img->bytes_per_line / 4;
    auto *pixels = reinterpret_cast<const uint8_t *>(img->data);
    if (texture_bgra_supported() && (tight || texture_row_length_supported())) {
        update_texture_rgba(cap.tex, static_cast<int>(width), static_cast<int>(height), pixels, false, tight ? 0 : stride_px, true);
    } else {
        static std::vector<uint8_t> scratch;
        swizzle_bgra_to_rgba(pixels, width, height, scratch);
        update_texture_rgba(cap.tex, static_cast<int>(width), static_cast<int>(height), scratch.data());
    }
    XDestroyImage(img);

    cap.last_capture = std::chrono::steady_clock::now();
    cap.in_flight = false;
}

void toplevel_export_prune(ToplevelExportState &state, const std::vector<std::string> &live_addresses) {
    auto *display = static_cast<Display *>(active_display());
    for (auto it = state.captures.begin(); it != state.captures.end();) {
        bool live = false;
        for (const auto &addr : live_addresses)
            if (addr == it->first) {
                live = true;
                break;
            }
        if (live) {
            ++it;
            continue;
        }
        release_pixmap(it->second, display);
        it = state.captures.erase(it);
    }
}

} // namespace backend_x11
