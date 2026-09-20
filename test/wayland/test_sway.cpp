#include <cassert>

#include "service/dock_service.h"
#include "service/sway_service.h"

namespace {

const char *kWorkspaces = R"([
  {"num": 1, "name": "1", "output": "DP-1", "visible": true, "focused": true},
  {"num": 2, "name": "2", "output": "DP-1", "visible": false, "focused": false},
  {"num": 11, "name": "11", "output": "HDMI-A-1", "visible": true, "focused": false},
  {"num": -1, "name": "scratch", "output": "DP-1", "visible": false, "focused": false}
])";

const char *kOutputs = R"([
  {"name": "DP-1", "active": true, "focused": true, "scale": 1.5, "transform": "normal",
   "rect": {"x": 0, "y": 0, "width": 1280, "height": 720},
   "current_mode": {"width": 1920, "height": 1080, "refresh": 60000}},
  {"name": "HDMI-A-1", "active": true, "focused": false, "scale": 1.0, "transform": "90",
   "rect": {"x": 1280, "y": 0, "width": 1080, "height": 1920},
   "current_mode": {"width": 1920, "height": 1080, "refresh": 60000}},
  {"name": "eDP-1", "active": false, "focused": false, "scale": 1.0, "transform": "normal",
   "rect": {"x": 0, "y": 0, "width": 0, "height": 0}}
])";

const char *kTree = R"({
  "type": "root", "name": "root", "nodes": [
    {"type": "output", "name": "__i3", "nodes": [
      {"type": "workspace", "name": "__i3_scratch", "num": -1, "nodes": [], "floating_nodes": [
        {"type": "floating_con", "id": 900, "name": "hidden", "app_id": "hidden", "nodes": [], "floating_nodes": [],
         "rect": {"x": 0, "y": 0, "width": 10, "height": 10}, "focused": false}
      ]}
    ]},
    {"type": "output", "name": "DP-1", "nodes": [
      {"type": "workspace", "name": "1", "num": 1, "nodes": [
        {"type": "con", "id": 10, "name": null, "nodes": [
          {"type": "con", "id": 11, "name": "term", "app_id": "kitty", "nodes": [], "floating_nodes": [],
           "rect": {"x": 200, "y": 0, "width": 400, "height": 700}, "focused": false},
          {"type": "con", "id": 12, "name": "browser", "app_id": null, "shell": "xwayland",
           "window_properties": {"class": "firefox"}, "nodes": [], "floating_nodes": [],
           "rect": {"x": 0, "y": 0, "width": 200, "height": 700}, "focused": true,
           "fullscreen_mode": 1, "sticky": true}
        ], "floating_nodes": []}
      ], "floating_nodes": [
        {"type": "floating_con", "id": 13, "name": "popup", "app_id": "mpv", "nodes": [], "floating_nodes": [],
         "rect": {"x": 50, "y": 50, "width": 300, "height": 200}, "focused": false}
      ]},
      {"type": "workspace", "name": "2", "num": 2, "nodes": [], "floating_nodes": []}
    ]},
    {"type": "output", "name": "HDMI-A-1", "nodes": [
      {"type": "workspace", "name": "11", "num": 11, "nodes": [
        {"type": "con", "id": 20, "name": "editor", "app_id": "code", "nodes": [], "floating_nodes": [],
         "rect": {"x": 1280, "y": 0, "width": 1080, "height": 1900}, "focused": false}
      ], "floating_nodes": []}
    ]}
  ]
})";

CompositorState parsed_state() {
    CompositorState state;
    sway_parse_workspaces(kWorkspaces, state);
    sway_parse_outputs(kOutputs, state);
    sway_parse_tree(kTree, state);
    return state;
}

const CompositorClient *find_client(const CompositorState &state, const std::string &address) {
    for (const CompositorClient &c : state.clients)
        if (c.address == address)
            return &c;
    return nullptr;
}

} // namespace

void test_sway() {
    CompositorState state = parsed_state();

    assert(state.by_monitor["DP-1"].workspaces.size() == 2);
    assert(state.by_monitor["DP-1"].active_id == 1);
    assert(state.by_monitor["DP-1"].workspaces[0].occupied);
    assert(!state.by_monitor["DP-1"].workspaces[1].occupied);
    assert(state.by_monitor["HDMI-A-1"].active_id == 11);
    assert(state.by_monitor["HDMI-A-1"].workspaces[0].occupied);

    assert(state.monitors.size() == 2);
    assert(state.focused_monitor == "DP-1");
    assert(state.monitors[0].id == 0);
    assert(state.monitors[0].width == 1920.0);
    assert(state.monitors[0].scale == 1.5);
    assert(state.monitors[0].transform == 0);
    assert(state.monitors[1].id == 1);
    assert(state.monitors[1].x == 1280.0);
    assert(state.monitors[1].transform == 1);

    assert(state.clients.size() == 4);
    assert(!find_client(state, "900"));

    const CompositorClient *term = find_client(state, "11");
    assert(term && term->window_class == "kitty");
    assert(term->workspace_id == 1 && term->monitor_id == 0);
    assert(term->at[0] == 200.0 && term->size[0] == 400.0);
    assert(!term->floating && !term->xwayland && term->focus_history_id > 0);

    const CompositorClient *browser = find_client(state, "12");
    assert(browser && browser->window_class == "firefox");
    assert(browser->xwayland && browser->pinned && browser->fullscreen == 1);
    assert(browser->focus_history_id == 0);

    const CompositorClient *popup = find_client(state, "13");
    assert(popup && popup->floating && popup->window_class == "mpv");

    const CompositorClient *editor = find_client(state, "20");
    assert(editor && editor->workspace_id == 11 && editor->monitor_id == 1);

    auto entries = dock_entries_for_monitor(state, "DP-1");
    assert(entries.size() == 3);
    assert(entries[0].window_class == "firefox" && entries[0].focused);
    assert(!entries[1].focused);
    assert(dock_entries_for_monitor(state, "HDMI-A-1").size() == 1);

    CompositorState bad;
    sway_parse_workspaces("not json", bad);
    sway_parse_outputs("not json", bad);
    sway_parse_tree("not json", bad);
    assert(bad.by_monitor.empty() && bad.monitors.empty() && bad.clients.empty());
}
