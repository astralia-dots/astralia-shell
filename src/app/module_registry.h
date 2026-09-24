#pragma once

#include <memory>
#include <vector>

#include "app/module.h"
#include "app/per_monitor_module.h"

std::vector<std::unique_ptr<Module>> build_app_modules();
std::vector<std::unique_ptr<PerMonitorModule>> build_per_monitor_modules();

void start_session_lock(WaylandState &app);
