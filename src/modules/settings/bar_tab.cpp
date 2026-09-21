#include <string_view>

#include "modules/settings/bar_tab.h"

using panel_chrome_detail::cached_text;

namespace {

constexpr std::string_view kBarStyleTagPrefix = "barstyle";

} // namespace

void bar_tab_paint(SettingsState &state, Node *root, int32_t scale, float x, float y, float w, const Config &cfg) {
    float tile_w = (w - kSettingsScreenSelectorSpacing * (kBarStyleCount - 1)) / kBarStyleCount;
    float cx = x;
    for (int i = 0; i < kBarStyleCount; ++i) {
        bool active = cfg.bar_style == static_cast<BarStyle>(i);
        node_add_rrect(root, cx, y, tile_w, kSettingsScreenSelectorHeight, kSettingsTileRadius, kSettingsSelectorBorderWidth, rgba(palette::lavender_alpha20), active ? rgba(palette::accent_alt) : kPanelNoBorder);
        const Texture *tex = cached_text(state.tcache, kBarStyleLabels[i], scale);
        if (tex)
            node_add_texture(root, cx + (tile_w - tex->width) / 2.0f, y + (kSettingsScreenSelectorHeight - tex->height) / 2.0f, *tex, rgba(palette::text));
        state.click_regions.push_back({PanelClickKind::ToggleFlip, {cx, y, tile_w, kSettingsScreenSelectorHeight}, std::string(kBarStyleTagPrefix) + std::to_string(i)});
        cx += tile_w + kSettingsScreenSelectorSpacing;
    }
}

bool bar_tab_handle_click(SettingsState &state, const Config &cfg, const SettingsCommitFn &on_commit, const PanelClickRegion &region) {
    if (region.kind != PanelClickKind::ToggleFlip || region.tag.compare(0, kBarStyleTagPrefix.size(), kBarStyleTagPrefix) != 0)
        return false;

    BarStyle style = static_cast<BarStyle>(std::stoi(region.tag.substr(kBarStyleTagPrefix.size())));
    if (cfg.bar_style != style) {
        settings_commit_focused_field(state, cfg, on_commit);
        Config updated = cfg;
        updated.bar_style = style;
        on_commit(updated);
        settings_request_frame(state);
    }
    return true;
}
