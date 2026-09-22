#pragma once

#include <cstdint>
#include <string>

#include "service/compositor_service.h"

void hypr_refresh(CompositorState &state);

bool hypr_refresh_clients(CompositorState &state);

int32_t hypr_bar_hug_radius_px(CompositorState &state);

bool hypr_init(CompositorState &state);

CompositorEventResult hypr_poll_events(CompositorState &state);

void hypr_dispatch(CompositorState &state, const std::string &command);

void hypr_tile_focus_workspace(CompositorState &state, int id, bool global = false);

void hypr_tile_move_window(CompositorState &state, int id, bool follow = true, const std::string &address = {}, bool global = false);

enum class HyprCloseScope { Workspace,
                            Monitor,
                            All };

void hypr_tile_close_workspace(CompositorState &state, HyprCloseScope scope, int id = -1);

void hypr_tile_move_workspace_in(CompositorState &state, int id, bool global = false);

void hypr_tile_swap_workspace(CompositorState &state, int id, bool global = false);
