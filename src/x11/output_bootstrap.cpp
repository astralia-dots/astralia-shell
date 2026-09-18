#include <cstdint>
#include <memory>
#include <unordered_map>

#include "x11/output_bootstrap.h"

#include "app/backend.h"
#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

namespace backend_x11 {

namespace {

std::unordered_map<void *, MonitorRect> &rect_table() {
    static std::unordered_map<void *, MonitorRect> table;
    return table;
}

} // namespace

bool bootstrap_outputs(WaylandState &app) {
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    if (!resources)
        return false;

    for (int i = 0; i < resources->noutput; ++i) {
        RROutput output_id = resources->outputs[i];
        XRROutputInfo *output_info = XRRGetOutputInfo(display, resources, output_id);
        if (!output_info)
            continue;
        if (output_info->connection != RR_Connected || output_info->crtc == None) {
            XRRFreeOutputInfo(output_info);
            continue;
        }
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, resources, output_info->crtc);
        if (!crtc_info) {
            XRRFreeOutputInfo(output_info);
            continue;
        }

        auto mon = std::make_unique<MonitorOutput>();
        mon->app = &app;
        mon->output.registry_name = static_cast<uint32_t>(output_id);
        mon->output.name = output_info->name;
        mon->output.wl = reinterpret_cast<wl_output *>(static_cast<uintptr_t>(output_id));
        mon->output.scale = 1;
        mon->output.done = true;

        rect_table()[mon->output.wl] = MonitorRect{
            crtc_info->x,
            crtc_info->y,
            static_cast<int32_t>(crtc_info->width),
            static_cast<int32_t>(crtc_info->height),
        };

        app.outputs.push_back(std::move(mon));

        XRRFreeCrtcInfo(crtc_info);
        XRRFreeOutputInfo(output_info);
    }

    XRRFreeScreenResources(resources);
    return !app.outputs.empty();
}

MonitorRect monitor_rect(void *output_token) {
    auto it = rect_table().find(output_token);
    if (it != rect_table().end())
        return it->second;
    auto *display = static_cast<Display *>(active_display());
    int screen = DefaultScreen(display);
    return MonitorRect{0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen)};
}

} // namespace backend_x11
