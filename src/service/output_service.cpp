#include "service/output_service.h"

#include "app/backend.h"

#include "wayland/output_service.h"

#include "x11/output_service.h"

void output_scale_watch(OutputScale &state, wl_surface *surface) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::output_scale_watch(state, surface);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::output_scale_watch(state, surface);
#endif
}
