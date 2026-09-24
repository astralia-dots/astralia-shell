#include <cstdint>

#include "config/bar_config.h"

#include "modules/bar/styles/islands.h"

#include "render/palette.h"
#include "render/panel_chrome.h"

namespace {

constexpr BarStyleSpec kIslandsStyle = {palette::overlay, palette::accent, metrics::border_thin, 0.5f, 0.5f, bar_detail::kBarTopMargin, static_cast<int32_t>(kPanelSideMargin), 0.0f, 0.0f, 0.0f};

} // namespace

const BarStyleSpec &islands_style_spec() {
    return kIslandsStyle;
}
