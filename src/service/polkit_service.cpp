#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <memory>
#include <optional>
#include <poll.h>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

#include "core/log.h"

#include "service/polkit_service.h"

namespace {

constexpr auto kAgentObjectPath = "/org/astralia_shell/PolkitAuthenticationAgent";

template <typename F>
void guard_polkit_callback(const char *name, F &&body) noexcept {
    try {
        std::forward<F>(body)();
    } catch (const std::exception &e) {
        klog("polkit: exception in callback %s (%s); ignoring", name, e.what());
    } catch (...) {
        klog("polkit: unknown exception in callback %s; ignoring", name);
    }
}

std::optional<std::string> username_from_uid(uid_t uid) {
    passwd pwd{};
    passwd *result = nullptr;
    std::array<char, 4096> buffer{};
    const int rc = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &result);
    if (rc != 0 || result == nullptr || result->pw_name == nullptr || result->pw_name[0] == '\0')
        return std::nullopt;
    return std::string(result->pw_name);
}

PolkitRequestIdentity to_request_identity(PolkitIdentity *identity) {
    PolkitRequestIdentity out;
    if (POLKIT_IS_UNIX_USER(identity)) {
        const auto uid = static_cast<uid_t>(polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity)));
        out.kind = "unix-user";
        out.uid = static_cast<std::uint32_t>(uid);
        out.user_name = username_from_uid(uid).value_or(std::to_string(uid));
    } else if (POLKIT_IS_UNIX_GROUP(identity)) {
        out.kind = "unix-group";
        out.uid = static_cast<std::uint32_t>(polkit_unix_group_get_gid(POLKIT_UNIX_GROUP(identity)));
    }
    return out;
}

std::string identity_display_name(PolkitIdentity *identity) {
    if (POLKIT_IS_UNIX_USER(identity)) {
        const auto uid = static_cast<uid_t>(polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity)));
        return username_from_uid(uid).value_or(std::to_string(uid));
    }
    if (POLKIT_IS_UNIX_GROUP(identity))
        return "group " + std::to_string(polkit_unix_group_get_gid(POLKIT_UNIX_GROUP(identity)));
    return "unknown";
}

bool is_no_session_for_pid_error(const char *message) {
    return message != nullptr && std::string_view(message).contains("No session for pid");
}

class IdentityRef {
  public:
    explicit IdentityRef(PolkitIdentity *identity = nullptr)
        : identity_(identity) {
        if (identity_ != nullptr)
            g_object_ref(identity_);
    }

    ~IdentityRef() {
        if (identity_ != nullptr)
            g_object_unref(identity_);
    }

    IdentityRef(const IdentityRef &) = delete;
    IdentityRef &operator=(const IdentityRef &) = delete;

    IdentityRef(IdentityRef &&other) noexcept
        : identity_(std::exchange(other.identity_, nullptr)) {}

    IdentityRef &operator=(IdentityRef &&other) noexcept {
        if (this == &other)
            return *this;
        if (identity_ != nullptr)
            g_object_unref(identity_);
        identity_ = std::exchange(other.identity_, nullptr);
        return *this;
    }

    [[nodiscard]] PolkitIdentity *get() const noexcept { return identity_; }

  private:
    PolkitIdentity *identity_ = nullptr;
};

struct InternalAuthRequest {
    std::string action_id;
    std::string message;
    std::string icon_name;
    std::string cookie;
    std::vector<IdentityRef> identities;
    GTask *task = nullptr;
    GCancellable *cancellable = nullptr;
    gulong cancel_handler_id = 0;
    bool finished = false;

    ~InternalAuthRequest() {
        if (cancellable != nullptr && cancel_handler_id != 0)
            g_cancellable_disconnect(cancellable, cancel_handler_id);
        if (cancellable != nullptr)
            g_object_unref(cancellable);
        if (!finished && task != nullptr)
            g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "%s", "Authentication request was destroyed");
        if (task != nullptr)
            g_object_unref(task);
    }

    void complete() {
        if (finished || task == nullptr)
            return;
        finished = true;
        g_task_return_boolean(task, TRUE);
    }

    void cancel(const char *reason) {
        if (finished || task == nullptr)
            return;
        finished = true;
        g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_CANCELLED, "%s", reason);
    }
};

using InitiateCallback = void (*)(void *, std::unique_ptr<InternalAuthRequest>);
using CancelCallback = void (*)(void *, InternalAuthRequest *);

} // namespace

using AstraliaShellPolkitListener = struct _AstraliaShellPolkitListener {
    PolkitAgentListener parent_instance;
    void *owner = nullptr;
    InitiateCallback initiate = nullptr;
    CancelCallback cancel = nullptr;
    gpointer registration_handle = nullptr;
};

using AstraliaShellPolkitListenerClass = struct _AstraliaShellPolkitListenerClass {
    PolkitAgentListenerClass parent_class;
};

static void astralia_shell_polkit_listener_initiate_authentication(PolkitAgentListener *listener, const gchar *action_id, const gchar *message, const gchar *icon_name, PolkitDetails *, const gchar *cookie, GList *identities, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) noexcept;
static gboolean astralia_shell_polkit_listener_initiate_authentication_finish(PolkitAgentListener *listener, GAsyncResult *result, GError **error);
static void astralia_shell_polkit_request_cancelled(GCancellable *cancellable, gpointer user_data) noexcept;

G_DEFINE_TYPE(AstraliaShellPolkitListener, astralia_shell_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

static void astralia_shell_polkit_listener_init(AstraliaShellPolkitListener *self) {
    self->owner = nullptr;
    self->initiate = nullptr;
    self->cancel = nullptr;
    self->registration_handle = nullptr;
}

static void astralia_shell_polkit_listener_class_init(AstraliaShellPolkitListenerClass *klass) {
    auto *listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
    listener_class->initiate_authentication =
        astralia_shell_polkit_listener_initiate_authentication;
    listener_class->initiate_authentication_finish =
        astralia_shell_polkit_listener_initiate_authentication_finish;
}

static void astralia_shell_polkit_listener_initiate_authentication(PolkitAgentListener *listener, const gchar *action_id, const gchar *message, const gchar *icon_name, PolkitDetails *, const gchar *cookie, GList *identities, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data) noexcept {
    guard_polkit_callback("initiate_authentication", [&]() {
        auto *self = reinterpret_cast<AstraliaShellPolkitListener *>(listener);
        auto request = std::make_unique<InternalAuthRequest>();
        request->action_id = action_id != nullptr ? action_id : "";
        request->message = message != nullptr ? message : "";
        request->icon_name = icon_name != nullptr ? icon_name : "";
        request->cookie = cookie != nullptr ? cookie : "";
        request->task =
            g_task_new(G_OBJECT(listener), nullptr, callback, user_data);
        request->cancellable =
            cancellable != nullptr
                ? static_cast<GCancellable *>(g_object_ref(cancellable))
                : nullptr;

        for (GList *item = g_list_first(identities); item != nullptr; item = g_list_next(item)) {
            auto *identity = static_cast<PolkitIdentity *>(item->data);
            if (identity == nullptr)
                continue;
            const auto duplicate = std::ranges::find_if(request->identities, [identity](const IdentityRef &existing) {
                return polkit_identity_equal(existing.get(), identity);
            });
            if (duplicate == request->identities.end())
                request->identities.emplace_back(identity);
        }

        if (cancellable != nullptr)
            request->cancel_handler_id = g_cancellable_connect(cancellable, G_CALLBACK(astralia_shell_polkit_request_cancelled), request.get(), nullptr);

        if (self->initiate == nullptr || self->owner == nullptr) {
            request->cancel("Polkit listener is not attached");
            return;
        }
        self->initiate(self->owner, std::move(request));
    });
}

static gboolean astralia_shell_polkit_listener_initiate_authentication_finish(PolkitAgentListener *, GAsyncResult *result, GError **error) {
    return g_task_propagate_boolean(G_TASK(result), error);
}

static void astralia_shell_polkit_request_cancelled(GCancellable *, gpointer user_data) noexcept {
    guard_polkit_callback("request_cancelled", [&]() {
        auto *request = static_cast<InternalAuthRequest *>(user_data);
        request->cancel_handler_id = 0;
        auto *source = G_IS_TASK(request->task) ? g_task_get_source_object(request->task) : nullptr;
        auto *listener = source != nullptr ? reinterpret_cast<AstraliaShellPolkitListener *>(source) : nullptr;
        if (listener != nullptr && listener->cancel != nullptr && listener->owner != nullptr)
            listener->cancel(listener->owner, request);
    });
}

struct PolkitAgent::Impl {
    StateCallback state_callback;
    ReadyCallback ready_callback;
    AstraliaShellPolkitListener *listener = nullptr;
    PolkitAgentSession *session = nullptr;
    GMainContext *context = nullptr;
    GCancellable *register_cancellable = nullptr;
    bool starting = false;
    bool registered = false;
    std::unique_ptr<InternalAuthRequest> pending;
    PolkitIdentity *active_identity = nullptr;
    bool cancelling = false;

    bool response_required = false;
    bool response_visible = false;
    std::string input_prompt;
    std::string supplementary_message;
    bool supplementary_error = false;

    mutable std::vector<GPollFD> glib_poll_fds;
    mutable gint glib_max_priority = G_PRIORITY_DEFAULT;
    mutable int glib_poll_timeout_ms = -1;

    Impl() : context(g_main_context_default()) {
        listener = static_cast<AstraliaShellPolkitListener *>(g_object_new(astralia_shell_polkit_listener_get_type(), nullptr));
        listener->owner = this;
        listener->initiate = &Impl::initiate_bridge;
        listener->cancel = &Impl::cancel_bridge;
    }

    ~Impl() {
        clear_pending("PolkitAgent is being destroyed", true);
        if (register_cancellable != nullptr) {
            g_cancellable_cancel(register_cancellable);
            g_object_unref(register_cancellable);
            register_cancellable = nullptr;
        }
        if (listener != nullptr) {
            listener->owner = nullptr;
            listener->initiate = nullptr;
            listener->cancel = nullptr;
            if (listener->registration_handle != nullptr) {
                polkit_agent_listener_unregister(listener->registration_handle);
                listener->registration_handle = nullptr;
            }
            g_object_unref(listener);
        }
    }

    static void session_ready_trampoline(GObject *source, GAsyncResult *result, gpointer user_data) noexcept {
        guard_polkit_callback("session_ready", [&]() {
            static_cast<Impl *>(user_data)->on_session_ready(source, result);
        });
    }

    void start() {
        if (starting || registered)
            return;
        starting = true;

        const char *session_id = std::getenv("XDG_SESSION_ID");
        register_cancellable = g_cancellable_new();
        if (session_id != nullptr && session_id[0] != '\0') {
            PolkitSubject *subject = polkit_unix_session_new(session_id);
            begin_register_subject(subject, nullptr);
            return;
        }

        polkit_unix_session_new_for_process(::getpid(), register_cancellable, &Impl::session_ready_trampoline, this);
    }

    void on_session_ready(GObject *, GAsyncResult *result) {
        GError *error = nullptr;

        PolkitSubject *pid_subject =
            polkit_unix_session_new_for_process_finish(result, &error);

        if (error != nullptr && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            if (pid_subject != nullptr)
                g_object_unref(pid_subject);
            return;
        }

        if (pid_subject == nullptr || error != nullptr) {
            const bool no_session =
                error != nullptr && is_no_session_for_pid_error(error->message);
            if (pid_subject != nullptr) {
                g_object_unref(pid_subject);
                pid_subject = nullptr;
            }
            if (no_session) {
                g_clear_error(&error);
                klog("polkit: no logind session for pid; trying unix-user "
                     "authentication agent");
                PolkitSubject *user_subject = POLKIT_SUBJECT(polkit_unix_user_new(static_cast<gint>(::getuid())));
                begin_register_subject(user_subject, nullptr);
                return;
            }
        }

        begin_register_subject(pid_subject, error);
    }

    void begin_register_subject(PolkitSubject *subject, GError *error) {
        starting = false;
        if (register_cancellable != nullptr) {
            g_object_unref(register_cancellable);
            register_cancellable = nullptr;
        }

        if (subject == nullptr || error != nullptr) {
            std::string message =
                error != nullptr ? error->message : "failed to create polkit session subject";
            g_clear_error(&error);
            if (subject != nullptr)
                g_object_unref(subject);
            if (ready_callback)
                ready_callback(false, message);
            return;
        }

        GError *register_error = nullptr;
        gpointer handle = polkit_agent_listener_register(POLKIT_AGENT_LISTENER(listener), POLKIT_AGENT_REGISTER_FLAGS_NONE, subject, kAgentObjectPath, nullptr, &register_error);
        g_object_unref(subject);

        if (register_error != nullptr) {
            const std::string message = register_error->message;
            g_clear_error(&register_error);
            if (handle != nullptr)
                polkit_agent_listener_unregister(handle);
            if (ready_callback)
                ready_callback(false, message);
            return;
        }
        if (handle == nullptr) {
            if (ready_callback)
                ready_callback(false, "polkit listener registration returned no handle");
            return;
        }

        if (listener->registration_handle != nullptr)
            polkit_agent_listener_unregister(listener->registration_handle);
        listener->registration_handle = handle;
        registered = true;
        klog("polkit: registered authentication agent at %s", kAgentObjectPath);
        if (ready_callback)
            ready_callback(true, std::string{});
    }

    static void initiate_bridge(void *owner, std::unique_ptr<InternalAuthRequest> request) {
        static_cast<Impl *>(owner)->begin_authentication(std::move(request));
    }

    static void cancel_bridge(void *owner, InternalAuthRequest *request) {
        static_cast<Impl *>(owner)->cancel_from_authority(request);
    }

    static void completed_callback(PolkitAgentSession *, gboolean gained_authorization, gpointer user_data) noexcept {
        guard_polkit_callback("completed", [&]() {
            static_cast<Impl *>(user_data)->handle_completed(gained_authorization != FALSE);
        });
    }

    static void request_callback(PolkitAgentSession *, gchar *request, gboolean echo_on, gpointer user_data) noexcept {
        guard_polkit_callback("request", [&]() {
            static_cast<Impl *>(user_data)->handle_request(request != nullptr ? request : "", echo_on != FALSE);
        });
    }

    static void show_error_callback(PolkitAgentSession *, gchar *text, gpointer user_data) noexcept {
        guard_polkit_callback("show_error", [&]() {
            static_cast<Impl *>(user_data)->set_supplementary(text != nullptr ? text : "", true);
        });
    }

    static void show_info_callback(PolkitAgentSession *, gchar *text, gpointer user_data) noexcept {
        guard_polkit_callback("show_info", [&]() {
            static_cast<Impl *>(user_data)->set_supplementary(text != nullptr ? text : "", false);
        });
    }

    void emit_state_changed() {
        if (state_callback)
            state_callback();
    }

    void clear_conversation_state() {
        response_required = false;
        response_visible = false;
        input_prompt.clear();
        supplementary_message.clear();
        supplementary_error = false;
    }

    void stop_session() {
        active_identity = nullptr;
        if (session != nullptr) {
            g_signal_handlers_disconnect_by_data(session, this);
            g_object_unref(session);
            session = nullptr;
        }
    }

    void clear_pending(const char *cancel_reason = "Authentication request cancelled", bool silent = false) {
        stop_session();
        if (pending != nullptr && !pending->finished)
            pending->cancel(cancel_reason);
        pending.reset();
        cancelling = false;
        clear_conversation_state();
        if (!silent)
            emit_state_changed();
    }

    void begin_authentication(std::unique_ptr<InternalAuthRequest> request) {
        if (pending != nullptr)
            clear_pending("Replaced by a newer authentication request", true);

        if (request->identities.empty()) {
            klog("polkit: request \"%s\" has no identities", request->action_id.c_str());
            request->cancel("Authentication request has no identities");
            return;
        }

        pending = std::move(request);
        clear_conversation_state();
        if (!start_session()) {
            klog("polkit: session startup failed for action \"%s\"", pending->action_id.c_str());
            clear_pending("Failed to start authentication session");
            return;
        }
        emit_state_changed();
    }

    bool start_session() {
        if (pending == nullptr)
            return false;

        stop_session();

        PolkitIdentity *chosen = nullptr;
        const auto self_uid = static_cast<gint>(::geteuid());
        for (const IdentityRef &identity : pending->identities) {
            if (!POLKIT_IS_UNIX_USER(identity.get()))
                continue;
            if (polkit_unix_user_get_uid(POLKIT_UNIX_USER(identity.get())) == self_uid) {
                chosen = identity.get();
                break;
            }
        }
        if (chosen == nullptr) {
            for (const IdentityRef &identity : pending->identities) {
                if (POLKIT_IS_UNIX_USER(identity.get())) {
                    chosen = identity.get();
                    break;
                }
            }
        }
        if (chosen == nullptr) {
            klog("polkit: action \"%s\" has no unix-user identity (unix-group "
                 "alone is unsupported)",
                 pending->action_id.c_str());
            return false;
        }

        active_identity = chosen;
        session =
            polkit_agent_session_new(active_identity, pending->cookie.c_str());
        if (session == nullptr)
            return false;

        g_signal_connect(G_OBJECT(session), "completed", G_CALLBACK(completed_callback), this);
        g_signal_connect(G_OBJECT(session), "request", G_CALLBACK(request_callback), this);
        g_signal_connect(G_OBJECT(session), "show-error", G_CALLBACK(show_error_callback), this);
        g_signal_connect(G_OBJECT(session), "show-info", G_CALLBACK(show_info_callback), this);

        polkit_agent_session_initiate(session);
        return true;
    }

    void handle_request(const std::string &prompt, bool echo_on) {
        input_prompt = prompt.empty() ? "Password:" : prompt;
        response_visible = echo_on;
        response_required = true;
        emit_state_changed();
    }

    void set_supplementary(const std::string &text, bool is_error) {
        supplementary_message = text;
        supplementary_error = is_error;
        emit_state_changed();
    }

    void handle_completed(bool gained_authorization) {
        if (pending == nullptr)
            return;

        if (gained_authorization) {
            klog("polkit: action \"%s\" authorized as %s", pending->action_id.c_str(), active_identity != nullptr ? identity_display_name(active_identity).c_str() : "unknown");
            pending->complete();
            pending.reset();
            stop_session();
            clear_conversation_state();
            emit_state_changed();
            return;
        }

        if (cancelling) {
            clear_pending("Authentication request cancelled");
            return;
        }

        response_required = false;
        response_visible = false;
        input_prompt.clear();
        supplementary_message = "Incorrect password, try again";
        supplementary_error = true;
        emit_state_changed();

        if (!start_session()) {
            klog("polkit: session restart failed for action \"%s\"", pending->action_id.c_str());
            clear_pending("Failed to restart authentication session");
        }
    }

    void cancel_from_authority(InternalAuthRequest *request) {
        if (pending == nullptr || pending.get() != request)
            return;
        cancelling = true;
        clear_pending("Authentication request cancelled by polkit");
    }

    void submit_response(const std::string &response) {
        if (pending == nullptr || session == nullptr || !response_required)
            return;
        if (response.empty())
            return;
        polkit_agent_session_response(session, response.c_str());
        response_required = false;
        input_prompt.clear();
        supplementary_message = "Authenticating...";
        supplementary_error = false;
        emit_state_changed();

        while (g_main_context_pending(context))
            g_main_context_iteration(context, FALSE);
    }

    void cancel_request() {
        if (pending == nullptr)
            return;
        cancelling = true;
        if (session != nullptr)
            polkit_agent_session_cancel(session);
        clear_pending("Authentication request cancelled by user");
    }

    std::size_t add_poll_fds(std::vector<pollfd> &fds) const {
        glib_poll_fds.clear();
        glib_max_priority = G_PRIORITY_DEFAULT;
        glib_poll_timeout_ms = -1;

        if (!g_main_context_acquire(context))
            return 0;

        const gboolean ready =
            g_main_context_prepare(context, &glib_max_priority);
        gint timeout = -1;
        const gint count = g_main_context_query(context, glib_max_priority, &timeout, nullptr, 0);
        glib_poll_timeout_ms = ready ? 0 : timeout;
        std::size_t added = 0;
        if (count > 0) {
            glib_poll_fds.resize(static_cast<std::size_t>(count));
            g_main_context_query(context, glib_max_priority, &timeout, glib_poll_fds.data(), count);
            glib_poll_timeout_ms = ready ? 0 : timeout;
            for (const GPollFD &glib_fd : glib_poll_fds) {
                fds.push_back({.fd = glib_fd.fd, .events = static_cast<short>(glib_fd.events), .revents = 0});
                ++added;
            }
        }
        g_main_context_release(context);
        return added;
    }

    int poll_timeout_ms() const {
        if (starting)
            return glib_poll_timeout_ms < 0 ? 100 : std::min(glib_poll_timeout_ms, 100);
        return glib_poll_timeout_ms;
    }

    void dispatch(const std::vector<pollfd> &fds, std::size_t start_idx) {
        if (!g_main_context_acquire(context))
            return;

        for (std::size_t i = 0; i < glib_poll_fds.size(); ++i) {
            const std::size_t poll_index = start_idx + i;
            glib_poll_fds[i].revents =
                poll_index < fds.size()
                    ? static_cast<gushort>(fds[poll_index].revents)
                    : static_cast<gushort>(0);
        }

        const gboolean ready = g_main_context_check(context, glib_max_priority, glib_poll_fds.data(), static_cast<gint>(glib_poll_fds.size()));
        g_main_context_release(context);

        if (ready) {
            if (!g_main_context_acquire(context))
                return;
            g_main_context_dispatch(context);
            g_main_context_release(context);
        }

        while (g_main_context_pending(context))
            g_main_context_iteration(context, FALSE);
    }

    PolkitRequest pending_request() const {
        if (pending == nullptr)
            return {};
        PolkitRequest request;
        request.action_id = pending->action_id;
        request.message = pending->message;
        request.icon_name = pending->icon_name;
        request.cookie = pending->cookie;
        request.identities.reserve(pending->identities.size());
        for (const IdentityRef &identity : pending->identities)
            request.identities.push_back(to_request_identity(identity.get()));
        return request;
    }
};

PolkitAgent::PolkitAgent() : impl_(std::make_unique<Impl>()) {}

PolkitAgent::~PolkitAgent() = default;

void PolkitAgent::start() {
    if (impl_ != nullptr)
        impl_->start();
}

void PolkitAgent::set_state_callback(StateCallback callback) {
    if (impl_ != nullptr)
        impl_->state_callback = std::move(callback);
}

void PolkitAgent::set_ready_callback(ReadyCallback callback) {
    if (impl_ != nullptr)
        impl_->ready_callback = std::move(callback);
}

void PolkitAgent::submit_response(const std::string &response) {
    if (impl_ != nullptr)
        impl_->submit_response(response);
}

void PolkitAgent::cancel_request() {
    if (impl_ != nullptr)
        impl_->cancel_request();
}

std::size_t PolkitAgent::add_poll_fds(std::vector<pollfd> &fds) const {
    return impl_ != nullptr ? impl_->add_poll_fds(fds) : 0;
}

int PolkitAgent::poll_timeout_ms() const {
    return impl_ != nullptr ? impl_->poll_timeout_ms() : -1;
}

void PolkitAgent::dispatch(const std::vector<pollfd> &fds, std::size_t start_idx) {
    if (impl_ != nullptr)
        impl_->dispatch(fds, start_idx);
}

bool PolkitAgent::has_pending_request() const noexcept {
    return impl_ != nullptr && impl_->pending != nullptr;
}

PolkitRequest PolkitAgent::pending_request() const {
    return impl_ != nullptr ? impl_->pending_request() : PolkitRequest{};
}

bool PolkitAgent::is_response_required() const noexcept {
    return impl_ != nullptr && impl_->response_required;
}

bool PolkitAgent::response_visible() const noexcept {
    return impl_ != nullptr && impl_->response_visible;
}

std::string PolkitAgent::input_prompt() const {
    return impl_ != nullptr ? impl_->input_prompt : std::string{};
}

std::string PolkitAgent::supplementary_message() const {
    return impl_ != nullptr ? impl_->supplementary_message : std::string{};
}

bool PolkitAgent::supplementary_is_error() const noexcept {
    return impl_ != nullptr && impl_->supplementary_error;
}
