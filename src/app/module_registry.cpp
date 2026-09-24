#include "app/module_registry.h"
#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "modules/bar.h"
#include "modules/dashboard.h"
#include "modules/idle.h"
#include "modules/launcher.h"
#include "modules/lock.h"
#include "modules/logout.h"
#include "modules/notification.h"
#include "modules/osd.h"
#include "modules/overview.h"
#include "modules/polkit.h"
#include "modules/rain.h"
#include "modules/settings.h"
#include "modules/visualizer.h"
#include "modules/wallpaper.h"

namespace {

void draw_monitor_wallpaper(MonitorOutput &mon, Node &root, int32_t w, int32_t h) {
    if (auto *wp = mon.module<WallpaperPerMonitorModule>())
        wallpaper_draw_columns(wp->wallpaper_state(), &root, w, h);
}

void draw_named_wallpaper(WaylandState &app, const std::string &output_name, Node &root, int32_t w, int32_t h) {
    for (auto &mon : app.outputs) {
        if (mon->output.name != output_name)
            continue;
        draw_monitor_wallpaper(*mon, root, w, h);
        return;
    }
}

MediaDecodeStatus wallpaper_decode_status(WaylandState &app, const std::string &output_name, int column) {
    for (auto &mon : app.outputs) {
        if (mon->output.name != output_name)
            continue;
        if (auto *wp = mon->module<WallpaperPerMonitorModule>())
            return wp->decode_status(column);
    }
    return MediaDecodeStatus::Idle;
}

void set_wallpaper_paused(MonitorOutput &mon, bool paused) {
    auto *wp = mon.module<WallpaperPerMonitorModule>();
    if (!wp)
        return;
    if (paused)
        wp->pause_animation();
    else
        wp->resume_animation();
}

} // namespace

std::vector<std::unique_ptr<Module>> build_app_modules() {
    std::vector<std::unique_ptr<Module>> modules;
    modules.push_back(make_launcher_module());
    modules.push_back(make_logout_module());
    modules.push_back(make_dashboard_module());
    modules.push_back(make_overview_module());
    modules.push_back(make_settings_module(wallpaper_decode_status));
    modules.push_back(make_rain_module());
    modules.push_back(make_visualizer_module());
    modules.push_back(make_lock_module(draw_named_wallpaper));
    modules.push_back(make_polkit_module());
    return modules;
}

std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules() {
    std::vector<std::unique_ptr<PerMonitorModule>> modules;
    modules.push_back(std::make_unique<BarPerMonitorModule>());
    modules.push_back(std::make_unique<WallpaperPerMonitorModule>());
    modules.push_back(make_osd_per_monitor_module());
    modules.push_back(std::make_unique<NotificationViewPerMonitorModule>());
    modules.push_back(make_idle_per_monitor_module({draw_monitor_wallpaper, set_wallpaper_paused}));
    return modules;
}

void start_session_lock(WaylandState &app) {
    lock_start(app);
}
