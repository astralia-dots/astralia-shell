#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "modules/bar.h"
#include "modules/bar/panel/battery_panel.h"
#include "modules/bar/panel/bluetooth_panel.h"
#include "modules/bar/panel/network_panel.h"
#include "modules/bar/panel/tray_panel.h"
#include "modules/bar/panel/volume_panel.h"
#include "modules/bar/widget/status_widget.h"

#include "render/icon.h"
#include "render/icons.h"

#include "service/bluetooth_service.h"
#include "service/network_service.h"
#include "service/pipewire_service.h"

namespace {

const char *wifi_icon_glyph(const NetworkState &n) {
    if (n.connectivity == "portal")
        return icon::lock;
    if (n.ethernet_connected)
        return icon::router;
    if (!n.connected_ssid().empty()) {
        int sig = n.connected_signal();
        if (sig > 75)
            return icon::wifi;
        if (sig > 50)
            return icon::wifi2;
        if (sig > 25)
            return icon::wifi1;
        return icon::wifi0;
    }
    if (n.ethernet_available && !n.wifi_available)
        return icon::router;
    return icon::wifi_off;
}

std::string wifi_label(const NetworkState &n) {
    if (n.connectivity == "portal")
        return "Sign in";
    std::string label = n.ethernet_connected ? "Ethernet" : n.connected_ssid();
    return label.empty() ? "Wi-Fi" : label;
}

const char *bluetooth_icon_glyph(const BluetoothState &b) {
    if (!b.adapter_present || !b.powered)
        return icon::bluetooth_off;
    for (const BluetoothDeviceInfo &d : b.devices)
        if (d.connected)
            return icon::bluetooth_connected;
    return icon::bluetooth_on;
}

std::string bluetooth_label(const BluetoothState &b) {
    if (!b.adapter_present)
        return "Unavailable";
    if (!b.powered)
        return "Disconnected";
    for (const BluetoothDeviceInfo &d : b.devices)
        if (d.connected)
            return d.name.empty() ? d.address : d.name;
    return "Idle";
}

const char *volume_icon_glyph(const PipewireState &pw) {
    bool muted = false;
    float level = pipewire_sink_level(pw, muted);
    return volume_threshold_icon(muted, level);
}

std::string volume_label(const PipewireState &pw) {
    bool muted = false;
    float level = pipewire_sink_level(pw, muted);
    if (muted)
        return "muted";
    return std::to_string(static_cast<int>(std::lround(level * 100))) + "%";
}

const char *battery_icon_glyph(const UpowerState &u) {
    if (u.full)
        return icon::plugged_in;
    if (u.charging)
        return icon::battery_charging;
    if (u.percent <= 25)
        return icon::battery1;
    if (u.percent <= 50)
        return icon::battery2;
    if (u.percent <= 75)
        return icon::battery3;
    return icon::battery4;
}

std::string battery_label(const UpowerState &u) {
    if (!u.present)
        return "No Battery";
    if (u.full)
        return "Plugged in";
    return std::to_string(u.percent) + "%";
}

} // namespace

namespace bar_detail {

namespace {

void open_tray_panel(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    close_other_overlays(mon, PillId::Tray);
    tray_menu_close(bs.tray_menu);
    if (!bs.tray_panel.base.open) {
        update_pill_expand(bs.capsule, mon.animations, PillId::Tray, true, true);
        bar_paint(mon);
        overlay_panel_ensure(bs.tray_panel.base, mon.app->display, [&] { return tray_panel_create_surface(bs.tray_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return tray_panel_init_egl(bs.tray_panel, mon.app->renderer, mon.app->tray, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
        app_detail::rest_egl_current(*mon.app);
    }
    tray_panel_toggle(bs.tray_panel, pill_center_x(bs.capsule, PillId::Tray));
}

void open_network_panel(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    close_other_overlays(mon, PillId::Wifi);
    if (!bs.network_panel.base.open) {
        update_pill_expand(bs.capsule, mon.animations, PillId::Wifi, true, true);
        bar_paint(mon);
        overlay_panel_ensure(bs.network_panel.base, mon.app->display, [&] { return network_panel_create_surface(bs.network_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return network_panel_init_egl(bs.network_panel, mon.app->renderer, mon.app->network, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
        app_detail::rest_egl_current(*mon.app);
    }
    network_panel_toggle(bs.network_panel, pill_center_x(bs.capsule, PillId::Wifi));
    if (bs.network_panel.base.open)
        network_scan(mon.app->network);
}

void open_bluetooth_panel(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    close_other_overlays(mon, PillId::Bluetooth);
    if (!bs.bluetooth_panel.base.open) {
        update_pill_expand(bs.capsule, mon.animations, PillId::Bluetooth, true, true);
        bar_paint(mon);
        overlay_panel_ensure(bs.bluetooth_panel.base, mon.app->display, [&] { return bluetooth_panel_create_surface(bs.bluetooth_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return bluetooth_panel_init_egl(bs.bluetooth_panel, mon.app->renderer, mon.app->bluetooth, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
        app_detail::rest_egl_current(*mon.app);
    }
    bluetooth_panel_toggle(bs.bluetooth_panel, mon.app->bluetooth, pill_center_x(bs.capsule, PillId::Bluetooth));
}

void open_volume_panel(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    close_other_overlays(mon, PillId::Volume);
    if (!bs.volume_panel.base.open) {
        update_pill_expand(bs.capsule, mon.animations, PillId::Volume, true, true);
        bar_paint(mon);
        overlay_panel_ensure(bs.volume_panel.base, mon.app->display, [&] { return volume_panel_create_surface(bs.volume_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return volume_panel_init_egl(bs.volume_panel, mon.app->renderer, mon.app->pipewire, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
        app_detail::rest_egl_current(*mon.app);
    }
    volume_panel_toggle(bs.volume_panel, pill_center_x(bs.capsule, PillId::Volume));
}

void open_battery_panel(MonitorOutput &mon) {
    BarPerMonitorState &bs = bar_state(mon);
    close_other_overlays(mon, PillId::Battery);
    if (!bs.battery_panel.base.open) {
        update_pill_expand(bs.capsule, mon.animations, PillId::Battery, true, true);
        bar_paint(mon);
        overlay_panel_ensure(bs.battery_panel.base, mon.app->display, [&] { return battery_panel_create_surface(bs.battery_panel, mon.app->compositor, mon.app->layer_shell, mon.output.wl); }, [&] { return battery_panel_init_egl(bs.battery_panel, mon.app->renderer, mon.app->upower, mon.app->egl_display, mon.app->egl_config, mon.app->egl_context); });
        app_detail::rest_egl_current(*mon.app);
    }
    battery_panel_toggle(bs.battery_panel, pill_center_x(bs.capsule, PillId::Battery));
}

} // namespace

std::vector<Pill> status_pills(MonitorOutput &mon) {
    StatusWidgetState &sw = bar_state(mon).status_widget;
    WaylandState &app = *mon.app;

    if (!sw.tray_icon_texture.id)
        sw.tray_icon_texture = make_icon_texture(icon::tray);
    const char *wifi_glyph = wifi_icon_glyph(app.network);
    if (wifi_glyph != sw.wifi_icon_glyph_cached) {
        sw.wifi_icon_texture = make_icon_texture(wifi_glyph);
        sw.wifi_icon_glyph_cached = wifi_glyph;
    }
    const char *bluetooth_glyph = app.bluetooth.adapter_present ? bluetooth_icon_glyph(app.bluetooth) : nullptr;
    if (bluetooth_glyph != sw.bluetooth_icon_glyph_cached) {
        sw.bluetooth_icon_texture = bluetooth_glyph ? make_icon_texture(bluetooth_glyph) : Texture{};
        sw.bluetooth_icon_glyph_cached = bluetooth_glyph;
    }
    const char *volume_glyph = volume_icon_glyph(app.pipewire);
    if (volume_glyph != sw.volume_icon_glyph_cached) {
        sw.volume_icon_texture = make_icon_texture(volume_glyph);
        sw.volume_icon_glyph_cached = volume_glyph;
    }
    const char *battery_glyph = app.upower.present ? battery_icon_glyph(app.upower) : nullptr;
    if (battery_glyph != sw.battery_icon_glyph_cached) {
        sw.battery_icon_texture = battery_glyph ? make_icon_texture(battery_glyph) : Texture{};
        sw.battery_icon_glyph_cached = battery_glyph;
    }

    return {
        Pill{PillId::Tray, &sw.tray_icon_texture, "Tray", nullptr, [&mon] { open_tray_panel(mon); }},
        Pill{PillId::Wifi, &sw.wifi_icon_texture, wifi_label(app.network), nullptr, [&mon] { open_network_panel(mon); }},
        Pill{PillId::Bluetooth, &sw.bluetooth_icon_texture, bluetooth_label(app.bluetooth), nullptr, [&mon] { open_bluetooth_panel(mon); }},
        Pill{PillId::Volume, &sw.volume_icon_texture, volume_label(app.pipewire), nullptr, [&mon] { open_volume_panel(mon); }},
        Pill{PillId::Battery, &sw.battery_icon_texture, battery_label(app.upower), nullptr, [&mon] { open_battery_panel(mon); }},
    };
}

void volume_pill_handle_wheel(MonitorOutput &mon, double dy) {
    PipewireState &pw = mon.app->pipewire;
    bool sink_muted = false;
    float level = pipewire_sink_level(pw, sink_muted);
    float step = dy < 0 ? 0.05f : -0.05f;
    float next = std::clamp(level + step, 0.0f, 1.5f);
    if (pw.default_sink_id != 0)
        pipewire_set_node_volume(pw, pw.default_sink_id, next);
}

void volume_pill_peek_tick(MonitorOutput &mon) {
    StatusWidgetState &sw = bar_state(mon).status_widget;
    auto now = std::chrono::steady_clock::now();
    if (!sw.volume_peek_ready) {
        if (now - sw.volume_peek_started_at >= kVolumePeekReadyDelayMs)
            sw.volume_peek_ready = true;
        else
            return;
    }

    bool muted = false;
    float level = pipewire_sink_level(mon.app->pipewire, muted);
    bool changed = sw.volume_peek_last_level < 0.0f || std::abs(level - sw.volume_peek_last_level) > 0.001f || muted != sw.volume_peek_last_muted;
    sw.volume_peek_last_level = level;
    sw.volume_peek_last_muted = muted;
    if (!changed)
        return;

    sw.volume_peek_active = true;
    sw.volume_peek_deadline = now + kVolumePeekMs;
}

bool volume_pill_peek_expire(MonitorOutput &mon) {
    StatusWidgetState &sw = bar_state(mon).status_widget;
    if (!sw.volume_peek_active)
        return false;
    if (std::chrono::steady_clock::now() < sw.volume_peek_deadline)
        return false;
    sw.volume_peek_active = false;
    return true;
}

} // namespace bar_detail
