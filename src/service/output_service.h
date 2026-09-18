#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct wl_output;
struct wl_surface;

struct OutputScale {
    int32_t scale = 1;
    std::function<void(int32_t)> on_change;
};

void output_scale_watch(OutputScale &state, wl_surface *surface);

struct Output {
    uint32_t registry_name = 0;
    wl_output *wl = nullptr;
    std::string name;
    int32_t scale = 1;
    bool done = false;
};

inline wl_output *active_output_select(const std::vector<Output *> &outputs, const std::string &focused_name, wl_output *pointer_hint) {
    if (!focused_name.empty()) {
        for (Output *o : outputs)
            if (o->name == focused_name)
                return o->wl;
    }
    if (pointer_hint)
        return pointer_hint;
    return outputs.empty() ? nullptr : outputs.front()->wl;
}
