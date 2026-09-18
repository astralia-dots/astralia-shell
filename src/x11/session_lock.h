#pragma once

#include "modules/lock.h"

namespace backend_x11 {

ext_session_lock_v1 *session_lock_acquire(WaylandState &app, LockState &st);
bool session_lock_create_surface(LockState &st, LockOutputSurface &los, wl_output *output);
void session_lock_destroy_surface(LockOutputSurface &los);
void session_lock_release(LockState &st);

} // namespace backend_x11
