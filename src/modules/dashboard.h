#pragma once

#include <memory>
#include <vector>

#include "app/ipc.h"
#include "app/module.h"

#include "config/dashboard_config.h"

#include "render/renderer.h"
#include "render/scene.h"
#include "render/toplevel_window.h"

#include "service/input_service.h"

struct WaylandState;

struct DashboardState {
    ToplevelWindowBase base;
    Renderer *renderer = nullptr;
    Scene scene;
};

void dashboard_request_frame(DashboardState &state);

void dashboard_toggle(DashboardState &state, WaylandState &app);

void dashboard_handle_key_event(DashboardState &state, WaylandState &app, const KeyEvent &event);

std::vector<IpcHandler> dashboard_ipc_handlers(DashboardState &dashboard, WaylandState &state);

void dashboard_paint(DashboardState &state);

std::unique_ptr<Module> make_dashboard_module();
