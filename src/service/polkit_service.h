#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <poll.h>
#include <string>
#include <vector>

#include "core/poll_source.h"

struct PolkitRequestIdentity {
    std::string kind;
    std::uint32_t uid = 0;
    std::string user_name;
};

struct PolkitRequest {
    std::string action_id;
    std::string message;
    std::string icon_name;
    std::string cookie;
    std::vector<PolkitRequestIdentity> identities;
};

class PolkitAgent {
  public:
    using StateCallback = std::function<void()>;
    using ReadyCallback =
        std::function<void(bool ok, const std::string &error)>;

    PolkitAgent();
    ~PolkitAgent();

    PolkitAgent(const PolkitAgent &) = delete;
    PolkitAgent &operator=(const PolkitAgent &) = delete;

    void start();

    void set_state_callback(StateCallback callback);
    void set_ready_callback(ReadyCallback callback);
    void submit_response(const std::string &response);
    void cancel_request();

    std::size_t add_poll_fds(std::vector<pollfd> &fds) const;
    [[nodiscard]] int poll_timeout_ms() const;
    void dispatch(const std::vector<pollfd> &fds, std::size_t start_idx);

    [[nodiscard]] bool has_pending_request() const noexcept;
    [[nodiscard]] PolkitRequest pending_request() const;
    [[nodiscard]] bool is_response_required() const noexcept;
    [[nodiscard]] bool response_visible() const noexcept;
    [[nodiscard]] std::string input_prompt() const;
    [[nodiscard]] std::string supplementary_message() const;
    [[nodiscard]] bool supplementary_is_error() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class PolkitPollSource final : public PollSource {
  public:
    explicit PolkitPollSource(PolkitAgent &agent) : agent_(agent) {}

    std::size_t add_poll_fds(std::vector<pollfd> &fds) override {
        return agent_.add_poll_fds(fds);
    }

    void dispatch(const std::vector<pollfd> &fds, std::size_t start_idx) override {
        agent_.dispatch(fds, start_idx);
    }

  private:
    PolkitAgent &agent_;
};
