#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct Workspace {
    int id = -1;
    std::string name;
    bool occupied = false;
};

struct MonitorWorkspaces {
    std::vector<Workspace> workspaces;
    int active_id = -1;
};

struct CompositorMonitor {
    int id = -1;
    std::string name;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double scale = 1.0;
    int transform = 0;
    std::array<double, 4> reserved{0.0, 0.0, 0.0, 0.0};
};

struct CompositorClient {
    std::string address;
    std::string window_class;
    std::string title;
    int workspace_id = -1;
    int monitor_id = -1;
    std::array<double, 2> at{0.0, 0.0};
    std::array<double, 2> size{0.0, 0.0};
    bool floating = false;
    int fullscreen = 0;
    bool pinned = false;
    long focus_history_id = 0;
    bool xwayland = false;
};

enum class CompositorBackend { None,
                               Hyprland,
                               Sway };

struct CompositorState {
    CompositorBackend backend = CompositorBackend::None;
    std::string request_socket_path;
    std::string event_socket_path;
    int event_fd = -1;
    std::unordered_map<std::string, MonitorWorkspaces> by_monitor;
    std::string focused_monitor;
    std::vector<CompositorMonitor> monitors;
    std::vector<CompositorClient> clients;
};

enum class CompositorEventResult {
    None,
    ActiveChanged,
    StructuralChanged,
    Disconnected
};

int compositor_connect_socket(const std::string &path);

bool compositor_init(CompositorState &state);

void compositor_refresh(CompositorState &state);

bool compositor_refresh_clients(CompositorState &state);

CompositorEventResult compositor_poll_events(CompositorState &state);

void compositor_focus_workspace(CompositorState &state, int id, bool global = false);

const char *compositor_name(const CompositorState &state);
