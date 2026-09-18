#pragma once

#include <cstdint>

struct WaylandState;

void bar_layer_surface_configure(void *data, int32_t width, int32_t height);

bool bootstrap_egl(WaylandState &state);
bool renderer_bootstrap_init(WaylandState &state);
