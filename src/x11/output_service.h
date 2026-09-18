#pragma once

#include "service/output_service.h"

struct wl_surface;

namespace backend_x11 {

void output_scale_watch(OutputScale &state, wl_surface *surface);

} // namespace backend_x11
