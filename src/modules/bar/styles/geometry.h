#pragma once

#include <cstdint>

#include "config/bar_config.h"

#include "render/palette.h"

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

const BarStyleSpec &bar_style_spec(BarStyle style);

inline bool bar_style_has_rail(const BarStyleSpec &style) {
    return style.rail_height > 0.0f;
}

namespace bar_detail {

struct BarGeometry {
    int32_t height;
    int32_t margin_top;
    int32_t exclusive_zone;
};

BarGeometry bar_autohide_geometry(bool autohide, bool collapsed, int32_t cfg_height, int32_t top_margin, int32_t hug_radius);

} // namespace bar_detail
