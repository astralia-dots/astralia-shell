#include "modules/settings/animation_tab.h"

void animation_tab_paint(SettingsState &state, Node *root, int32_t scale, float x, float y, float w, const Config &cfg) {
    draw_toggle_row(state, root, scale, x, y, w, "Disable Animations", cfg.animations_disabled, "animationsdisabled", true);
}

bool animation_tab_handle_click(SettingsState &state, const Config &cfg, const SettingsCommitFn &on_commit, const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip || region.tag != "animationsdisabled")
        return false;

    Config updated = cfg;
    updated.animations_disabled = !cfg.animations_disabled;
    on_commit(updated);
    settings_request_frame(state);
    return true;
}
