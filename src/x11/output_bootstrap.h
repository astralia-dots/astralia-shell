#pragma once

#include <cstdint>

struct WaylandState;

namespace backend_x11 {

bool bootstrap_outputs(WaylandState &app);

struct MonitorRect {
    int32_t x = 0, y = 0, width = 0, height = 0;
};

MonitorRect monitor_rect(void *output_token);

} // namespace backend_x11
