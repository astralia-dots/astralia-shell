#include "service/capture_service.h"

#include "app/backend.h"

#include "wayland/capture_service.h"

#include "x11/capture_service.h"

void toplevel_export_request(ToplevelExportState &state, void *manager, void *shm, const std::string &address, int min_interval_ms) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::toplevel_export_request(state, manager, shm, address, min_interval_ms);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::toplevel_export_request(state, manager, shm, address, min_interval_ms);
#endif
}

const Texture *toplevel_export_texture(const ToplevelExportState &state, const std::string &address) {
    auto it = state.captures.find(address);
    if (it == state.captures.end() || !it->second.tex.id)
        return nullptr;
    return &it->second.tex;
}

void toplevel_export_prune(ToplevelExportState &state, const std::vector<std::string> &live_addresses) {
#ifdef ASTRALIA_HAVE_WAYLAND
    if (active_backend() == Backend::Wayland) {
        backend_wayland::toplevel_export_prune(state, live_addresses);
        return;
    }
#endif
#ifdef ASTRALIA_HAVE_X11
    backend_x11::toplevel_export_prune(state, live_addresses);
#endif
}
