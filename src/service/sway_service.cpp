#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unistd.h>

#include "core/log.h"

#include "service/sway_service.h"

namespace {

constexpr char kMagic[] = "i3-ipc";
constexpr size_t kMagicLen = sizeof(kMagic) - 1;
constexpr size_t kHeaderLen = kMagicLen + 2 * sizeof(uint32_t);

constexpr uint32_t kRunCommand = 0;
constexpr uint32_t kSubscribe = 2;
constexpr uint32_t kGetWorkspaces = 1;
constexpr uint32_t kGetOutputs = 3;
constexpr uint32_t kGetTree = 4;
constexpr uint32_t kEventMask = 0x80000000u;

constexpr const char *kSubscription = "[\"workspace\",\"window\",\"output\"]";
constexpr const char *kSwapTempWorkspace = "astralia_swap_tmp";
constexpr int kWorkspaceCount = 10;

void pad_workspaces(CompositorState &state) {
    for (const CompositorMonitor &mon : state.monitors) {
        std::vector<Workspace> &workspaces = state.by_monitor[mon.name].workspaces;

        std::array<bool, kWorkspaceCount + 1> present{};
        for (const Workspace &ws : workspaces)
            if (ws.id >= 1 && ws.id <= kWorkspaceCount)
                present[static_cast<size_t>(ws.id)] = true;

        for (int id = 1; id <= kWorkspaceCount; ++id) {
            if (present[static_cast<size_t>(id)])
                continue;
            Workspace ws;
            ws.id = id;
            ws.name = std::to_string(id);
            workspaces.push_back(std::move(ws));
        }

        std::sort(workspaces.begin(), workspaces.end(), [](const Workspace &a, const Workspace &b) { return a.id < b.id; });
    }
}

std::string frame(uint32_t type, const std::string &payload) {
    std::string out(kMagic, kMagicLen);
    uint32_t len = static_cast<uint32_t>(payload.size());
    out.append(reinterpret_cast<const char *>(&len), sizeof(len));
    out.append(reinterpret_cast<const char *>(&type), sizeof(type));
    out += payload;
    return out;
}

bool send_all(int fd, const std::string &data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool recv_exact(int fd, char *dst, size_t len) {
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, dst + got, len - got, 0);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        got += static_cast<size_t>(n);
    }
    return true;
}

bool recv_frame(int fd, uint32_t &type, std::string &payload) {
    char header[kHeaderLen];
    if (!recv_exact(fd, header, kHeaderLen) || memcmp(header, kMagic, kMagicLen) != 0)
        return false;
    uint32_t len;
    memcpy(&len, header + kMagicLen, sizeof(len));
    memcpy(&type, header + kMagicLen + sizeof(len), sizeof(type));
    payload.resize(len);
    return len == 0 || recv_exact(fd, payload.data(), len);
}

std::string request(const std::string &socket_path, uint32_t type, const std::string &payload = {}) {
    int fd = compositor_connect_socket(socket_path);
    if (fd < 0)
        return {};

    std::string reply;
    uint32_t reply_type;
    if (!send_all(fd, frame(type, payload)) || !recv_frame(fd, reply_type, reply))
        reply.clear();
    close(fd);
    return reply;
}

int transform_from_string(const std::string &t) {
    if (t == "90")
        return 1;
    if (t == "180")
        return 2;
    if (t == "270")
        return 3;
    if (t == "flipped")
        return 4;
    if (t == "flipped-90")
        return 5;
    if (t == "flipped-180")
        return 6;
    if (t == "flipped-270")
        return 7;
    return 0;
}

int monitor_index(const CompositorState &state, const std::string &name) {
    for (const CompositorMonitor &m : state.monitors)
        if (m.name == name)
            return m.id;
    return -1;
}

std::string str_field(const nlohmann::json &node, const char *key) {
    auto it = node.find(key);
    return it != node.end() && it->is_string() ? it->get<std::string>() : std::string();
}

struct TreeWalk {
    CompositorState &state;
    long next_history = 1;
    double output_x = 0.0;
    double output_y = 0.0;
    double output_w = 0.0;
    double output_h = 0.0;
};

void walk_tree(TreeWalk &walk, const nlohmann::json &node, int monitor_id, int workspace_id, bool floating) {
    using nlohmann::json;

    const json empty = json::array();
    const json &nodes = node.contains("nodes") ? node["nodes"] : empty;
    const json &floats = node.contains("floating_nodes") ? node["floating_nodes"] : empty;
    std::string type = str_field(node, "type");

    if (type == "root") {
        for (const json &child : nodes)
            walk_tree(walk, child, monitor_id, workspace_id, false);
        return;
    }
    if (type == "output") {
        int id = monitor_index(walk.state, str_field(node, "name"));
        if (id < 0)
            return;
        json rect = node.value("rect", json::object());
        walk.output_x = rect.value("x", 0.0);
        walk.output_y = rect.value("y", 0.0);
        walk.output_w = rect.value("width", 0.0);
        walk.output_h = rect.value("height", 0.0);
        for (const json &child : nodes)
            walk_tree(walk, child, id, workspace_id, false);
        return;
    }
    if (type == "workspace") {
        int num = node.value("num", -1);
        if (num < 0)
            return;
        json rect = node.value("rect", json::object());
        double left = rect.value("x", walk.output_x) - walk.output_x;
        double top = rect.value("y", walk.output_y) - walk.output_y;
        double right = walk.output_x + walk.output_w - (rect.value("x", walk.output_x) + rect.value("width", walk.output_w));
        double bottom = walk.output_y + walk.output_h - (rect.value("y", walk.output_y) + rect.value("height", walk.output_h));
        walk.state.monitors[static_cast<size_t>(monitor_id)].reserved = {std::max(0.0, left), std::max(0.0, top), std::max(0.0, right), std::max(0.0, bottom)};
        for (const json &child : nodes)
            walk_tree(walk, child, monitor_id, num, false);
        for (const json &child : floats)
            walk_tree(walk, child, monitor_id, num, true);
        return;
    }

    if (!nodes.empty() || !floats.empty()) {
        for (const json &child : nodes)
            walk_tree(walk, child, monitor_id, workspace_id, floating);
        for (const json &child : floats)
            walk_tree(walk, child, monitor_id, workspace_id, true);
        return;
    }

    CompositorClient c;
    c.address = std::to_string(node.value("id", 0LL));
    c.window_class = str_field(node, "app_id");
    if (c.window_class.empty() && node.contains("window_properties") && node["window_properties"].is_object())
        c.window_class = str_field(node["window_properties"], "class");
    c.title = str_field(node, "name");
    c.workspace_id = workspace_id;
    c.monitor_id = monitor_id;
    json rect = node.value("rect", json::object());
    c.at = {rect.value("x", 0.0), rect.value("y", 0.0)};
    c.size = {rect.value("width", 0.0), rect.value("height", 0.0)};
    c.floating = floating;
    c.fullscreen = node.value("fullscreen_mode", 0);
    c.pinned = node.value("sticky", false);
    c.xwayland = str_field(node, "shell") == "xwayland";
    c.focus_history_id = node.value("focused", false) ? 0 : walk.next_history++;
    walk.state.clients.push_back(std::move(c));
}

bool event_is_structural(uint32_t type, const std::string &payload) {
    using nlohmann::json;

    json body = json::parse(payload, nullptr, false);
    if (!body.is_object())
        return false;
    std::string change = str_field(body, "change");
    if (type == (kEventMask | 1))
        return true;
    if (type == (kEventMask | 0))
        return change == "focus" || change == "init" || change == "empty" || change == "rename" || change == "move" || change == "reload";
    if (type == (kEventMask | 3))
        return change == "new" || change == "close" || change == "focus" || change == "move" || change == "floating" || change == "fullscreen_mode";
    return false;
}

bool connect_events(CompositorState &state) {
    int fd = compositor_connect_socket(state.event_socket_path);
    if (fd < 0)
        return false;

    uint32_t reply_type;
    std::string reply;
    if (!send_all(fd, frame(kSubscribe, kSubscription)) || !recv_frame(fd, reply_type, reply)) {
        close(fd);
        return false;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    state.event_fd = fd;
    return true;
}

} // namespace

void sway_parse_workspaces(const std::string &reply, CompositorState &state) {
    using nlohmann::json;

    try {
        json arr = json::parse(reply);
        for (auto &w : arr) {
            Workspace ws;
            ws.id = w.value("num", -1);
            ws.name = str_field(w, "name");
            if (ws.id < 0)
                continue;
            std::string output = str_field(w, "output");
            if (w.value("visible", false))
                state.by_monitor[output].active_id = ws.id;
            state.by_monitor[output].workspaces.push_back(std::move(ws));
        }
        for (auto &entry : state.by_monitor)
            std::sort(entry.second.workspaces.begin(), entry.second.workspaces.end(), [](const Workspace &a, const Workspace &b) { return a.id < b.id; });
    } catch (const json::exception &e) {
        klog("sway: failed to parse get_workspaces: %s", e.what());
    }
}

void sway_parse_outputs(const std::string &reply, CompositorState &state) {
    using nlohmann::json;

    state.monitors.clear();
    try {
        json arr = json::parse(reply);
        for (auto &o : arr) {
            if (!o.value("active", true))
                continue;
            CompositorMonitor m;
            m.id = static_cast<int>(state.monitors.size());
            m.name = str_field(o, "name");
            json rect = o.value("rect", json::object());
            m.x = rect.value("x", 0.0);
            m.y = rect.value("y", 0.0);
            m.scale = o.value("scale", 1.0);
            m.transform = transform_from_string(str_field(o, "transform"));
            json mode = o.value("current_mode", json::object());
            m.width = mode.value("width", rect.value("width", 0.0) * m.scale);
            m.height = mode.value("height", rect.value("height", 0.0) * m.scale);
            if (o.value("focused", false))
                state.focused_monitor = m.name;
            state.monitors.push_back(std::move(m));
        }
    } catch (const json::exception &e) {
        klog("sway: failed to parse get_outputs: %s", e.what());
    }
}

void sway_parse_tree(const std::string &reply, CompositorState &state) {
    using nlohmann::json;

    state.clients.clear();
    try {
        TreeWalk walk{state};
        walk_tree(walk, json::parse(reply), -1, -1, false);
    } catch (const json::exception &e) {
        klog("sway: failed to parse get_tree: %s", e.what());
    }

    for (auto &entry : state.by_monitor)
        for (Workspace &ws : entry.second.workspaces)
            ws.occupied = std::any_of(state.clients.begin(), state.clients.end(), [&](const CompositorClient &c) { return c.workspace_id == ws.id; });
}

void sway_refresh(CompositorState &state) {
    state.by_monitor.clear();
    if (state.request_socket_path.empty())
        return;

    sway_parse_workspaces(request(state.request_socket_path, kGetWorkspaces), state);
    sway_parse_outputs(request(state.request_socket_path, kGetOutputs), state);
    sway_parse_tree(request(state.request_socket_path, kGetTree), state);
    pad_workspaces(state);
}

bool sway_init(CompositorState &state) {
    const char *sock = getenv("SWAYSOCK");
    if (!sock || !*sock) {
        klog("sway: SWAYSOCK not set, skipping compositor integration");
        return false;
    }
    state.request_socket_path = sock;
    state.event_socket_path = sock;
    sway_refresh(state);
    if (!connect_events(state)) {
        klog("sway: failed to subscribe to events: %s", strerror(errno));
        return false;
    }
    return true;
}

CompositorEventResult sway_poll_events(CompositorState &state) {
    static std::string read_buffer;

    char buf[4096];
    ssize_t n;
    while ((n = recv(state.event_fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
        read_buffer.append(buf, static_cast<size_t>(n));
    if (n == 0)
        return CompositorEventResult::Disconnected;
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return CompositorEventResult::Disconnected;

    CompositorEventResult result = CompositorEventResult::None;
    while (read_buffer.size() >= kHeaderLen) {
        uint32_t len;
        uint32_t type;
        memcpy(&len, read_buffer.data() + kMagicLen, sizeof(len));
        memcpy(&type, read_buffer.data() + kMagicLen + sizeof(len), sizeof(type));
        if (memcmp(read_buffer.data(), kMagic, kMagicLen) != 0) {
            read_buffer.clear();
            break;
        }
        if (read_buffer.size() < kHeaderLen + len)
            break;
        if (event_is_structural(type, read_buffer.substr(kHeaderLen, len)))
            result = CompositorEventResult::StructuralChanged;
        read_buffer.erase(0, kHeaderLen + len);
    }
    return result;
}

void sway_focus_workspace(CompositorState &state, int id) {
    if (state.request_socket_path.empty())
        return;
    request(state.request_socket_path, kRunCommand, "workspace number " + std::to_string(id));
}

namespace {

int focused_active_workspace(const CompositorState &state) {
    auto it = state.by_monitor.find(state.focused_monitor);
    return it != state.by_monitor.end() ? it->second.active_id : -1;
}

std::string workspace_number_target(int id) {
    return "number " + std::to_string(id);
}

std::string move_clients_command(const CompositorState &state, int from_workspace, const std::string &target) {
    std::string command;
    for (const CompositorClient &c : state.clients)
        if (c.workspace_id == from_workspace)
            command += "[con_id=" + c.address + "] move container to workspace " + target + "; ";
    return command;
}

std::string focus_command(int id) {
    return "workspace --no-auto-back-and-forth " + workspace_number_target(id);
}

} // namespace

void sway_move_window(CompositorState &state, const std::string &address, int id) {
    if (state.request_socket_path.empty() || address.empty())
        return;
    std::string command = "[con_id=" + address + "] move container to workspace " + workspace_number_target(id);
    int active = focused_active_workspace(state);
    if (active >= 0)
        command += "; " + focus_command(active);
    request(state.request_socket_path, kRunCommand, command);
}

void sway_move_workspace_in(CompositorState &state, int id) {
    int src = focused_active_workspace(state);
    if (state.request_socket_path.empty() || src < 0 || src == id)
        return;
    request(state.request_socket_path, kRunCommand, move_clients_command(state, src, workspace_number_target(id)) + focus_command(id));
}

void sway_swap_workspace(CompositorState &state, int id) {
    int src = focused_active_workspace(state);
    if (state.request_socket_path.empty() || src < 0 || src == id)
        return;
    std::string src_to_tmp = move_clients_command(state, src, kSwapTempWorkspace);
    std::string dst_to_src = move_clients_command(state, id, workspace_number_target(src));
    if (src_to_tmp.empty() && dst_to_src.empty())
        return;
    std::string tmp_to_dst;
    for (const CompositorClient &c : state.clients)
        if (c.workspace_id == src)
            tmp_to_dst += "[con_id=" + c.address + "] move container to workspace " + workspace_number_target(id) + "; ";
    request(state.request_socket_path, kRunCommand, src_to_tmp + dst_to_src + tmp_to_dst + focus_command(id));
}

void sway_close_window(CompositorState &state, const std::string &address) {
    if (state.request_socket_path.empty() || address.empty())
        return;
    request(state.request_socket_path, kRunCommand, "[con_id=" + address + "] kill");
}
