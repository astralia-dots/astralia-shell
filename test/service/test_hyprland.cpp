#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "service/hyprland_service.h"

namespace {

constexpr const char *kOneClient = R"([{"address": "0x1", "class": "kitty", "title": "t", "workspace": {"id": 1}, "monitor": 0, "at": [10, 0]}])";

class OneShotServer {
  public:
    OneShotServer(const std::string &path, const char *reply, int hold_ms) {
        unlink(path.c_str());
        listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        int bound = bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        int listening = listen(listen_fd_, 1);
        assert(bound == 0 && listening == 0);
        (void)bound;
        (void)listening;
        thread_ = std::thread([this, reply, hold_ms] {
            int fd = accept(listen_fd_, nullptr, nullptr);
            char buf[64];
            while (read(fd, buf, sizeof(buf)) > 0) {
            }
            if (reply)
                write(fd, reply, strlen(reply));
            std::this_thread::sleep_for(std::chrono::milliseconds(hold_ms));
            close(fd);
        });
    }

    ~OneShotServer() {
        thread_.join();
        close(listen_fd_);
    }

  private:
    int listen_fd_ = -1;
    std::thread thread_;
};

class SequencedServer {
  public:
    SequencedServer(const std::string &path, std::vector<std::string> replies) {
        unlink(path.c_str());
        listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        int bound = bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        int listening = listen(listen_fd_, static_cast<int>(replies.size()));
        assert(bound == 0 && listening == 0);
        (void)bound;
        (void)listening;
        thread_ = std::thread([this, replies] {
            for (const std::string &reply : replies) {
                int fd = accept(listen_fd_, nullptr, nullptr);
                char buf[64];
                while (read(fd, buf, sizeof(buf)) > 0) {
                }
                write(fd, reply.data(), reply.size());
                close(fd);
            }
        });
    }

    ~SequencedServer() {
        thread_.join();
        close(listen_fd_);
    }

  private:
    int listen_fd_ = -1;
    std::thread thread_;
};

std::string socket_path() {
    return (std::filesystem::temp_directory_path() / ("astralia-test-hypr-" + std::to_string(getpid()) + ".sock")).string();
}

} // namespace

void test_hyprland() {
    std::string path = socket_path();

    {
        CompositorState state;
        state.request_socket_path = path;
        state.clients.push_back({});
        state.clients.back().address = "0xkeep";
        OneShotServer server(path, nullptr, 400);
        auto start = std::chrono::steady_clock::now();
        bool changed = hypr_refresh_clients(state);
        auto elapsed = std::chrono::steady_clock::now() - start;
        assert(!changed);
        assert(elapsed < std::chrono::milliseconds(300));
        assert(state.clients.size() == 1 && state.clients[0].address == "0xkeep");
    }

    {
        CompositorState state;
        state.request_socket_path = path;
        OneShotServer server(path, kOneClient, 0);
        assert(hypr_refresh_clients(state));
        assert(state.clients.size() == 1 && state.clients[0].address == "0x1");
    }

    {
        CompositorState state;
        state.request_socket_path = path;
        state.clients_reply = kOneClient;
        OneShotServer server(path, kOneClient, 0);
        assert(!hypr_refresh_clients(state));
        assert(state.clients.empty());
    }

    {
        CompositorState state;
        state.request_socket_path = path;
        SequencedServer server(path, {R"({"option": "general:gaps_out", "css": "20 20 20 20", "set": true})",
                                      R"({"option": "decoration:rounding", "int": 10, "set": true})"});
        assert(hypr_bar_hug_radius_px(state) == 30);
    }

    {
        CompositorState state;
        assert(hypr_bar_hug_radius_px(state) == 0);
    }

    unlink(path.c_str());
}
