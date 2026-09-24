#include "app/user_info.h"

#include "modules/bar.h"
#include "modules/bar/panel/control_center_panel.h"
#include "modules/bar/widget/control_center_widget.h"

namespace bar_detail {

Pill control_center_pill(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    return Pill{PillId::ControlCenter, &bs.control_center_texture, user_info::username(), nullptr, [&mon, &bs] {
                    close_other_overlays(mon, PillId::ControlCenter);
                    if (!bs.control_center_panel.base.open) {
                        update_pill_expand(bs.capsule, bs.animations, PillId::ControlCenter, true, true);
                        bar_paint(mon);
                        overlay_panel_ensure(bs.control_center_panel.base, mon.app->display, [&] { return control_center_panel_create_surface(bs.control_center_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return control_center_panel_init_egl(bs.control_center_panel, mon.app->renderer, *mon.app, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
                        app_detail::rest_egl_current(*mon.app);
                    }
                    control_center_panel_toggle(bs.control_center_panel);
                }};
}

} // namespace bar_detail
