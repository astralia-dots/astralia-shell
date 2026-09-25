#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "config/bar_config.h"

#include "modules/bar/widget/widget_capsule.h"

#include "render/animation.h"
#include "render/rect.h"
#include "render/texture.h"

#include "service/compositor_service.h"

struct WorkspaceWidgetState {
    std::unordered_map<int, float> width_t;
    std::unordered_map<int, bool> was_active;
    std::vector<std::pair<int, Rect>> pill_hits;
    Rect overview_hit{};
};

namespace bar_detail {
float draw_workspace_row(Node *root, WorkspaceWidgetState &wstate, AnimationManager &animations, float x, float height, const std::vector<Workspace> &ws_list, int active_id, const BarStyleSpec &style, const Texture &overview_icon);

int workspace_row_hit_workspace(const WorkspaceWidgetState &wstate, float x, float y);

bool workspace_row_hit_overview(const WorkspaceWidgetState &wstate, float x, float y);

} // namespace bar_detail
