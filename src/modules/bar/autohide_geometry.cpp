#include "config/bar_config.h"

#include "modules/bar/autohide_geometry.h"

namespace bar_detail {

BarGeometry bar_autohide_geometry(bool autohide, bool collapsed, int32_t cfg_height, int32_t top_margin, int32_t hug_radius) {
    if (!autohide)
        return {cfg_height + hug_radius, top_margin, cfg_height};
    if (collapsed)
        return {kAutoHideStripPx, 0, 0};
    return {top_margin + cfg_height + hug_radius, 0, 0};
}

} // namespace bar_detail
