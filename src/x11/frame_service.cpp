#include "x11/frame_service.h"

namespace backend_x11 {

void frame_clock_drop_callback(FrameClock &clock) {
    clock.redraw_requested = false;
}

void request_frame(FrameClock &clock) {
    clock.mapped = true;
    clock.draw();
}

} // namespace backend_x11
