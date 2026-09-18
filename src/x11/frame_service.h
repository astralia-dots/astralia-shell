#pragma once

#include "service/frame_service.h"

namespace backend_x11 {

void request_frame(FrameClock &clock);
void frame_clock_drop_callback(FrameClock &clock);

} // namespace backend_x11
