#include "modules/bar/widget/resource_widget.h"
#include "modules/bar.h"
#include "modules/bar/panel/resource_panel.h"

namespace bar_detail {

Pill cpu_pill(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    return Pill{
        PillId::Cpu, &bs.cpu_texture, "Resource", nullptr, [&mon, &bs] {
            close_other_overlays(mon, PillId::Cpu);
            if (!bs.resource_panel.base.open) {
                update_pill_expand(bs.capsule, mon.animations, PillId::Cpu, true, true);
                bar_paint(mon);
                overlay_panel_ensure(bs.resource_panel.base, mon.app->display, [&] { return resource_panel_create_surface(bs.resource_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return resource_panel_init_egl(bs.resource_panel, mon.app->renderer, mon.app->cpu_temp, mon.app->gpu_temp, mon.app->system_stats, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
                app_detail::rest_egl_current(*mon.app);
            }
            resource_panel_toggle(bs.resource_panel, pill_center_x(bs.capsule, PillId::Cpu));
            if (bs.resource_panel.base.open) {
                cpu_temp_poll(mon.app->cpu_temp);
                gpu_temp_poll(mon.app->gpu_temp);
                system_stats_poll(mon.app->system_stats);
            }
        }};
}

} // namespace bar_detail
