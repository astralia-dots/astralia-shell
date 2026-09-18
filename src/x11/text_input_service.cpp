#include "service/text_input_service.h"

TextInputService::~TextInputService() { cleanup(); }

bool TextInputService::bind(zwp_text_input_manager_v3 *, wl_seat *) {
    return false;
}

void TextInputService::cleanup() {
    if (active_client_)
        deactivate_client(active_client_);
    manager_ = nullptr;
    seat_ = nullptr;
    entered_surface_ = nullptr;
    keyboard_focus_surface_ = nullptr;
    active_surface_ = nullptr;
    active_client_ = nullptr;
    pending_edit_ = {};
    commit_serial_ = 0;
    enabled_ = false;
}

void TextInputService::set_focused_client(wl_surface *, TextInputClient *) {
}

void TextInputService::clear_focused_client(TextInputClient *) {
}

void TextInputService::on_keyboard_focus_surface(wl_surface *, bool) {
}

void TextInputService::handle_enter(wl_surface *) {
}

void TextInputService::handle_leave(wl_surface *) {
}

void TextInputService::handle_preedit_string(const char *) {
}

void TextInputService::handle_commit_string(const char *) {
}

void TextInputService::handle_delete_surrounding_text(uint32_t, uint32_t) {
}

void TextInputService::handle_done(uint32_t) {
}

bool TextInputService::active_surface_accepts_text_input() const {
    return false;
}

void TextInputService::enable_active() {
}

void TextInputService::disable_active() {
}

void TextInputService::commit_active_state(bool) {
}

void TextInputService::commit_protocol_state() {
}

void TextInputService::deactivate_client(TextInputClient *client) {
    client->text_input_reset_preedit();
    client->text_input_deactivated(*this);
}
