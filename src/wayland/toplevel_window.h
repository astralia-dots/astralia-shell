#pragma once

#include "render/toplevel_window.h"

namespace backend_wayland {

bool toplevel_window_create_surface(ToplevelWindowBase &base, void *compositor, void *wm_base, const char *title, const char *app_id, int32_t default_width, int32_t default_height);
bool toplevel_window_init_egl(ToplevelWindowBase &base, EGLDisplay display, EGLConfig config, EGLContext context);
void toplevel_window_request_frame(ToplevelWindowBase &base);
void toplevel_window_destroy_surface(ToplevelWindowBase &base);

} // namespace backend_wayland
