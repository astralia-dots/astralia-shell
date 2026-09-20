#pragma once

#include <chrono>
#include <vector>

#include "modules/bar/widget/widget_capsule.h"

#include "render/texture.h"

struct MonitorOutput;

struct StatusWidgetState {
    Texture wifi_icon_texture;
    const char *wifi_icon_glyph_cached = nullptr;
    Texture bluetooth_icon_texture;
    const char *bluetooth_icon_glyph_cached = nullptr;
    Texture volume_icon_texture;
    const char *volume_icon_glyph_cached = nullptr;
    Texture battery_icon_texture;
    const char *battery_icon_glyph_cached = nullptr;

    bool volume_peek_active = false;
    bool volume_peek_ready = false;
    std::chrono::steady_clock::time_point volume_peek_started_at =
        std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point volume_peek_deadline{};
    float volume_peek_last_level = -1.0f;
    bool volume_peek_last_muted = false;
};

namespace bar_detail {

std::vector<Pill> status_pills(MonitorOutput &mon);

bool volume_pill_peek_expire(MonitorOutput &mon);
void volume_pill_peek_tick(MonitorOutput &mon);
void volume_pill_handle_wheel(MonitorOutput &mon, double dy);

} // namespace bar_detail
