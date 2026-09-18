#pragma once

#include <functional>

#include "render/egl_surface.h"

struct FrameClock {
    NativeSurfaceHandle surface = nullptr;
    void *callback = nullptr;
    bool redraw_requested = false;
    bool mapped = false;
    std::function<void()> draw;
};

void request_frame(FrameClock &clock);

void frame_clock_drop_callback(FrameClock &clock);
