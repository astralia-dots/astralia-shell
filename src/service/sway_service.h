#pragma once

#include <string>

#include "service/compositor_service.h"

bool sway_init(CompositorState &state);

void sway_refresh(CompositorState &state);

CompositorEventResult sway_poll_events(CompositorState &state);

void sway_focus_workspace(CompositorState &state, int id);

void sway_move_window(CompositorState &state, const std::string &address, int id);

void sway_close_window(CompositorState &state, const std::string &address);

void sway_move_workspace_in(CompositorState &state, int id);

void sway_swap_workspace(CompositorState &state, int id);

void sway_parse_workspaces(const std::string &reply, CompositorState &state);

void sway_parse_outputs(const std::string &reply, CompositorState &state);

void sway_parse_tree(const std::string &reply, CompositorState &state);
