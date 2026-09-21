#pragma once

#include <cstdint>

#include "config/bar_config.h"

#include "render/palette.h"
#include "render/panel_chrome.h"

struct BarStyleSpec {
    Color bg;
    Color border;
    float border_width;
    float radius_ratio;
    float pad_ratio;
    int32_t top_margin;
    int32_t side_margin;
    float rail_height;
    float island_radius;
    float fillet_radius;
};

inline constexpr BarStyleSpec kBarStyleSpecs[kBarStyleCount] = {
    {palette::overlay, palette::accent, metrics::border_thin, 0.5f, 0.5f, bar_detail::kBarTopMargin, static_cast<int32_t>(kPanelSideMargin), 0.0f, 0.0f, 0.0f},
    {palette::base, palette::accent, metrics::border_thin, 0.0f, 0.25f, 0, 0, 6.0f, 16.0f, 12.0f},
};

inline const BarStyleSpec &bar_style_spec(BarStyle style) {
    return kBarStyleSpecs[static_cast<int>(style)];
}

inline bool bar_style_has_rail(const BarStyleSpec &style) {
    return style.rail_height > 0.0f;
}
