#include "x11/bootstrap.h"
#include "x11/output_bootstrap.h"

#include "app/wayland_state.h"

#include "core/log.h"

#include "service/input_service.h"

namespace backend_x11 {

bool bootstrap(WaylandState &app) {
    if (!bootstrap_outputs(app)) {
        klog("no connected XRandR output found");
        return false;
    }

    app.seat_caps.keyboard = &app.keyboard;
    app.seat_caps.pointer = &app.pointer;
    keyboard_attach_seat(app.seat_caps, nullptr);
    pointer_bind(app.pointer, nullptr);
    return true;
}

} // namespace backend_x11
