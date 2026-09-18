#pragma once

#include "service/output_service.h"

struct wl_surface;

namespace backend_wayland {

void output_scale_watch(OutputScale &state, wl_surface *surface);

} // namespace backend_wayland
