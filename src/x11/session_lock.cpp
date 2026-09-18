#include "x11/session_lock.h"

#include "core/log.h"

namespace backend_x11 {

ext_session_lock_v1 *session_lock_acquire(WaylandState &, LockState &) {
    klog("lock: session lock is not yet supported under X11");
    return nullptr;
}

bool session_lock_create_surface(LockState &, LockOutputSurface &, wl_output *) {
    return false;
}

void session_lock_destroy_surface(LockOutputSurface &) {
}

void session_lock_release(LockState &) {
}

} // namespace backend_x11
