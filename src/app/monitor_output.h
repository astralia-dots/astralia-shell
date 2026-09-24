#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>

#include "app/per_monitor_module.h"
#include "app/wayland_state.h"

#include "service/compositor_service.h"
#include "service/media_service.h"
#include "service/output_service.h"

struct MonitorOutput {
    WaylandState *app = nullptr;
    Output output;
    bool activated = false;
    std::vector<std::unique_ptr<PerMonitorModule>> modules;

    template <typename T>
    T *module() const {
        for (auto &m : modules)
            if (T *t = dynamic_cast<T *>(m.get()))
                return t;
        return nullptr;
    }
};

void monitor_output_destroy(MonitorOutput &mon);
void monitor_output_create_surfaces(WaylandState &app, MonitorOutput &mon);
void monitor_output_wait_configured(WaylandState &app, MonitorOutput &mon);
void monitor_output_finish_egl(WaylandState &app, MonitorOutput &mon);
void monitor_output_activate(WaylandState &app, MonitorOutput &mon);
void request_all_frames(MonitorOutput &mon);

MonitorOutput *find_monitor_by_name_wl(WaylandState &app, wl_output *wl);
MonitorOutput *find_monitor_for_surface(WaylandState &app, wl_surface *surface);

struct SettingsState;

struct SettingsEnv {
    std::function<std::vector<std::string>()> monitor_names_fn;
    std::function<std::string()> focused_monitor_fn;
    std::function<MediaDecodeStatus(const std::string &, int)> decode_status_fn;
};

SettingsEnv settings_env(WaylandState &app);

namespace app_detail {

const std::vector<Workspace> &monitor_workspaces(const MonitorOutput &mon);

int monitor_active_workspace_id(const MonitorOutput &mon);

void rest_egl_current(WaylandState &app);

void apply_config_update(WaylandState &app, Config new_cfg);
void save_and_apply_config_update(WaylandState &app, Config new_cfg);
MonitorOutput *active_target_monitor(WaylandState &app);
void settings_retarget(WaylandState &app, SettingsState &settings, MonitorOutput &target);

} // namespace app_detail
