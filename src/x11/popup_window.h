#pragma once

#include "render/popup_window.h"

namespace backend_x11 {

bool popup_window_create(PopupWindowBase &base, void *compositor, void *wm_base, void *parent_layer, Rect anchor_rect, int32_t w, int32_t h, void *seat, uint32_t grab_serial);
bool popup_window_init_egl(PopupWindowBase &base, void *display, EGLDisplay egl_display, EGLConfig config, EGLContext context);
void popup_window_reposition(PopupWindowBase &base, void *wm_base, Rect anchor_rect, int32_t w, int32_t h);
void popup_window_request_frame(PopupWindowBase &base);
void popup_window_destroy(PopupWindowBase &base);

} // namespace backend_x11
