#pragma once

#include "service/frame_service.h"

namespace backend_wayland {

void request_frame(FrameClock &clock);
void frame_clock_drop_callback(FrameClock &clock);

} // namespace backend_wayland
