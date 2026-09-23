#include <cstdio>
#include <iterator>

void test_config();
void test_config_watch();
void test_monitor_overrides();
void test_wallpaper_resolve();
void test_async_process();
void test_deferred_call();
void test_path_home();
void test_poll_source();
void test_network_parse();
void test_bluetooth();
void test_spawn_helpers();
void test_desktop_entry();
void test_visit_store();
void test_apps_provider();
void test_files_provider();
void test_search();
void test_submenu();
void test_launch_action();
void test_icon_theme();
void test_keyboard();
void test_active_output();
void test_dock();
void test_sway();
void test_hyprland();
void test_rfkill();
void test_cpu_temp();
void test_gpu_temp();
void test_system_stats();
void test_mpris();
void test_animation();
void test_animated_image();
void test_marquee_scroll();
void test_palette();
void test_image_decode();
void test_text_elide();
void test_bar_fillet();
void test_bar_autohide_geometry();
void test_lock_layout();
void test_visualizer_fft();

int main() {
    struct Case {
        const char *name;
        void (*fn)();
    };
    Case cases[] = {
        {"config", test_config},
        {"config_watch", test_config_watch},
        {"monitor_overrides", test_monitor_overrides},
        {"wallpaper_resolve", test_wallpaper_resolve},

        {"async_process", test_async_process},
        {"deferred_call", test_deferred_call},
        {"path_home", test_path_home},
        {"poll_source", test_poll_source},

        {"network_parse", test_network_parse},
        {"bluetooth", test_bluetooth},

        {"spawn_helpers", test_spawn_helpers},
        {"desktop_entry", test_desktop_entry},
        {"visit_store", test_visit_store},
        {"apps_provider", test_apps_provider},
        {"files_provider", test_files_provider},
        {"search", test_search},
        {"submenu", test_submenu},
        {"launch_action", test_launch_action},
        {"icon_theme", test_icon_theme},

        {"keyboard", test_keyboard},
        {"active_output", test_active_output},
        {"dock", test_dock},
        {"sway", test_sway},
        {"hyprland", test_hyprland},

        {"rfkill", test_rfkill},
        {"cpu_temp", test_cpu_temp},
        {"gpu_temp", test_gpu_temp},
        {"system_stats", test_system_stats},
        {"mpris", test_mpris},

        {"animation", test_animation},
        {"animated_image", test_animated_image},
        {"marquee_scroll", test_marquee_scroll},
        {"palette", test_palette},
        {"image_decode", test_image_decode},
        {"text_elide", test_text_elide},
        {"bar_fillet", test_bar_fillet},
        {"bar_autohide_geometry", test_bar_autohide_geometry},
        {"lock_layout", test_lock_layout},
        {"visualizer_fft", test_visualizer_fft},
    };
    for (auto &c : cases) {
        std::printf("[ RUN ] %s\n", c.name);
        c.fn();
        std::printf("[ OK  ] %s\n", c.name);
    }
    std::printf("All %zu tests passed.\n", std::size(cases));
}
