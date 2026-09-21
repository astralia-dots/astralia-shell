#include "config/bar_config.h"
#include "config/dock_config.h"

#include "modules/bar/widget/dock_widget.h"

#include "render/palette.h"

namespace bar_detail {

float draw_dock_capsule(Node *root, DockWidgetState &st, AnimationManager &animations, float x, float height, const std::vector<DockEntry> &entries, const BarStyleSpec &style) {
    if (entries.empty())
        return x;

    float row_w = dock_row_width(entries);
    float pad = height * style.pad_ratio;
    float capsule_w = row_w + 2.0f * pad;
    if (!bar_style_has_rail(style))
        node_add_rrect(root, x, 0.0f, capsule_w, height, height * style.radius_ratio, style.border_width, rgba(style.bg), rgba(style.border));
    draw_dock_row(root, st.icons, st.row, animations, x + pad, height / 2.0f, entries, kDockWidgetAnimOwnerBase);
    return x + capsule_w + kCapsuleGap;
}

} // namespace bar_detail
