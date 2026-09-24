#include "config/bar_config.h"

#include "modules/bar/styles/geometry.h"
#include "modules/bar/styles/islands.h"
#include "modules/bar/styles/okinami.h"

const BarStyleSpec &bar_style_spec(BarStyle style) {
    if (style == BarStyle::Okinami)
        return okinami_style_spec();
    return islands_style_spec();
}

namespace bar_detail {

BarGeometry bar_autohide_geometry(bool autohide, bool collapsed, int32_t cfg_height, int32_t top_margin, int32_t hug_radius) {
    if (!autohide)
        return {cfg_height + hug_radius, top_margin, cfg_height};
    if (collapsed)
        return {kAutoHideStripPx, 0, 0};
    return {top_margin + cfg_height + hug_radius, 0, 0};
}

} // namespace bar_detail
