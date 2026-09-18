#include "app/backend_bootstrap.h"

#include "app/backend.h"

#include "wayland/bootstrap.h"

#include "x11/bootstrap.h"

bool backend_bootstrap(WaylandState &app) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland)
        return backend_wayland::bootstrap(app);
#endif
#ifdef ASTRALIA_HAVE_X11
    return backend_x11::bootstrap(app);
#else
    return false;
#endif
}
