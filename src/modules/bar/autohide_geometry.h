#pragma once

#include <cstdint>

namespace bar_detail {

struct BarGeometry {
    int32_t height;
    int32_t margin_top;
    int32_t exclusive_zone;
};

BarGeometry bar_autohide_geometry(bool autohide, bool collapsed, int32_t cfg_height, int32_t top_margin, int32_t hug_radius);

} // namespace bar_detail
