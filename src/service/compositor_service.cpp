#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/log.h"

#include "service/compositor_service.h"
#include "service/hyprland_service.h"
#include "service/sway_service.h"

int compositor_connect_socket(const std::string &path) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool compositor_init(CompositorState &state) {
    const char *sway_sock = getenv("SWAYSOCK");
    if (sway_sock && *sway_sock) {
        if (sway_init(state))
            state.backend = CompositorBackend::Sway;
    } else if (hypr_init(state)) {
        state.backend = CompositorBackend::Hyprland;
    }
    klog("compositor backend: %s", compositor_name(state));
    return state.backend != CompositorBackend::None;
}

void compositor_refresh(CompositorState &state) {
    if (state.backend == CompositorBackend::Sway)
        sway_refresh(state);
    else if (state.backend == CompositorBackend::Hyprland)
        hypr_refresh(state);
}

bool compositor_refresh_clients(CompositorState &state) {
    return state.backend == CompositorBackend::Hyprland && hypr_refresh_clients(state);
}

CompositorEventResult compositor_poll_events(CompositorState &state) {
    if (state.backend == CompositorBackend::Sway)
        return sway_poll_events(state);
    if (state.backend == CompositorBackend::Hyprland)
        return hypr_poll_events(state);
    return CompositorEventResult::None;
}

void compositor_focus_workspace(CompositorState &state, int id, bool global) {
    if (state.backend == CompositorBackend::Sway)
        sway_focus_workspace(state, id);
    else if (state.backend == CompositorBackend::Hyprland)
        hypr_tile_focus_workspace(state, id, global);
}

void compositor_move_window(CompositorState &state, const std::string &address, int id, bool global) {
    if (state.backend == CompositorBackend::Sway)
        sway_move_window(state, address, id);
    else if (state.backend == CompositorBackend::Hyprland)
        hypr_tile_move_window(state, id, false, address, global);
}

void compositor_close_window(CompositorState &state, const std::string &address) {
    if (state.backend == CompositorBackend::Sway)
        sway_close_window(state, address);
}

void compositor_move_workspace_in(CompositorState &state, int id, bool global) {
    if (state.backend == CompositorBackend::Sway)
        sway_move_workspace_in(state, id);
    else if (state.backend == CompositorBackend::Hyprland)
        hypr_tile_move_workspace_in(state, id, global);
}

void compositor_swap_workspace(CompositorState &state, int id, bool global) {
    if (state.backend == CompositorBackend::Sway)
        sway_swap_workspace(state, id);
    else if (state.backend == CompositorBackend::Hyprland)
        hypr_tile_swap_workspace(state, id, global);
}

const char *compositor_name(const CompositorState &state) {
    switch (state.backend) {
    case CompositorBackend::Hyprland:
        return "Hyprland";
    case CompositorBackend::Sway:
        return "Sway";
    case CompositorBackend::None:
        break;
    }
    return "Wayland";
}
