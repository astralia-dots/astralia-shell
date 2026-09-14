project(
  'adastria-shell',
  'c',
  'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++23', 'warning_level=3'],
)

wayland_client_dep = dependency('wayland-client')
wayland_egl_dep    = dependency('wayland-egl')
egl_dep            = dependency('egl')
glesv2_dep         = dependency('glesv2') # libGLESv2 exports the ES 3.2 entry points; the shell targets #version 320 es
cairo_dep          = dependency('cairo')
pangocairo_dep     = dependency('pangocairo')
nlohmann_json_dep  = dependency('nlohmann_json')
sdbus_cpp_dep      = dependency('sdbus-c++')
freetype2_dep      = dependency('freetype2')
libpipewire_dep    = dependency('libpipewire-0.3', include_type: 'system')
xkbcommon_dep      = dependency('xkbcommon')
pam_dep            = meson.get_compiler('cpp').find_library('pam')
libpng_dep         = dependency('libpng')
libjpeg_dep        = dependency('libjpeg')
librsvg_dep        = dependency('librsvg-2.0')
threads_dep        = dependency('threads')
libavformat_dep    = dependency('libavformat')
libavcodec_dep     = dependency('libavcodec')
libavfilter_dep    = dependency('libavfilter')
libavutil_dep      = dependency('libavutil')
# Only for DRM_FORMAT_* fourcc constants used in the zero-copy VAAPI dma-buf
# import; the VAAPI calls stay inside FFmpeg's own hwcontext_vaapi.
libdrm_dep         = dependency('libdrm')
glib_dep           = dependency('glib-2.0')
gio_dep            = dependency('gio-2.0')
gobject_dep        = dependency('gobject-2.0')
polkit_gobject_dep = dependency('polkit-gobject-1')
polkit_agent_dep   = dependency('polkit-agent-1')

adastria_shell_inc = include_directories('src')

adastria_shell_data_dir   = get_option('prefix') / get_option('datadir') / 'adastria-shell'
adastria_shell_font_dir   = adastria_shell_data_dir / 'fonts'
adastria_shell_bullet_dir = adastria_shell_data_dir / 'constellation'

install_data('assets/fonts/tabler-icons.ttf', install_dir: adastria_shell_font_dir) # shell-wide icons
install_data('assets/fonts/YujiMai.ttf', install_dir: adastria_shell_font_dir)      # logout module
install_data('assets/stellar-restoration.png', install_dir: adastria_shell_data_dir)            # default wallpaper
install_data('assets/stellar-restoration.svg', install_dir: adastria_shell_data_dir)         # idle screensaver logo (placeholder)
install_data('assets/stiletto.svg', install_dir: adastria_shell_data_dir)
install_data('assets/electro.png', install_dir: adastria_shell_data_dir)            # password echo glyph
install_data('assets/gifs/profile.gif', install_dir: adastria_shell_data_dir / 'gifs') # lock avatar + settings pfp
install_data('assets/logout/logo.gif', install_dir: adastria_shell_data_dir / 'logout') # logout centre logo (animated)
install_data('assets/logout/logo.png', install_dir: adastria_shell_data_dir / 'logout') # logout centre logo (static)
install_data('assets/pam/adastria-shell', install_dir: adastria_shell_data_dir / 'pam')    # lock PAM service
install_subdir('assets/shaders', install_dir: adastria_shell_data_dir)              # all GLES shader sources
foreach n : ['1', '2', '3', '4', '5', '6']                                   # keqing constellations
  install_data('assets/constellation/C' + n + '.png', install_dir: adastria_shell_bullet_dir)
endforeach

wayland_scanner = find_program('wayland-scanner')
wl_proto_dir = dependency('wayland-protocols').get_variable(pkgconfig: 'pkgdatadir')

# base => [xml, needs_sed]. The sed fixup renames the 'namespace' arg (a C++
# keyword) that wlr-layer-shell uses, matching noctalia's fix. Some protocols
# are listed only because another's requests reference their types and the
# generated code must still link: xdg-shell (layer-shell get_popup),
# tablet-unstable-v2 (cursor-shape), wlr-foreign-toplevel-management
# (toplevel-export v2) - nothing here calls them directly.
protocol_defs = [
  ['xdg-shell',                                   wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml',                     false],
  ['wlr-layer-shell-unstable-v1',                 'protocols/wlr-layer-shell-unstable-v1.xml',                         true],
  ['ext-idle-notify-v1',                          wl_proto_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml',     false],
  ['cursor-shape-v1',                             wl_proto_dir / 'staging/cursor-shape/cursor-shape-v1.xml',           false],
  ['tablet-unstable-v2',                          wl_proto_dir / 'unstable/tablet/tablet-unstable-v2.xml',             false],
  ['hyprland-toplevel-export-v1',                 'protocols/hyprland-toplevel-export-v1.xml',                         false],
  ['wlr-foreign-toplevel-management-unstable-v1', 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',         false],
  ['text-input-unstable-v3',                      'protocols/text-input-unstable-v3.xml',                              false],
  ['ext-session-lock-v1',                         wl_proto_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml',   false],
]

sed_fix = ' && sed -i s/namespace/name_space/g "$3"'
proto = {}
foreach p : protocol_defs
  base = p[0]
  fixup = p[2] ? sed_fix : ''
  proto += {base: [
    custom_target(base + '-client-header',
      input: p[1], output: base + '-client-protocol.h',
      command: ['sh', '-c', '"$1" client-header "$2" "$3"' + fixup,
                '_', wayland_scanner.full_path(), '@INPUT@', '@OUTPUT@']),
    custom_target(base + '-client-source',
      input: p[1], output: base + '-client-protocol.c',
      command: ['sh', '-c', '"$1" private-code "$2" "$3"' + fixup,
                '_', wayland_scanner.full_path(), '@INPUT@', '@OUTPUT@']),
  ]}
endforeach

proto_gen_all = []
foreach _name, tgts : proto
  proto_gen_all += tgts
endforeach

# Test binary links only the protocols its sources actually reference.
proto_gen_test = [
  proto['wlr-layer-shell-unstable-v1'],
  proto['xdg-shell'],
  proto['cursor-shape-v1'],
  proto['tablet-unstable-v2'],
]

# .h/.cpp module pairs shared between the main executable and the test binary;
# header-only .hpp modules need no entry. *_test_sources are exercised by
# test/**; *_main_only_sources pull in EGL/GL/cairo/pango/pipewire or generated
# protocol code and have no test coverage.
adastria_shell_core_split_sources = [
  'src/core/log.cpp',
  'src/core/poll_source.cpp',
  'src/core/deferred_call.cpp',
  'src/core/async_process.cpp',
  'src/core/path_home.cpp',
]

adastria_shell_render_test_sources = [
  'src/render/animation.cpp',
  'src/render/marquee_scroll.cpp',
  'src/render/texture.cpp',
  'src/render/image.cpp',
  'src/render/text_elide.cpp',
]

adastria_shell_render_main_only_sources = [
  'src/render/gl.cpp',
  'src/render/renderer.cpp',
  'src/render/node.cpp',
  'src/render/text.cpp',
  'src/render/icon.cpp',
  'src/render/texture_cache.cpp',
  'src/render/panel_scroll.cpp',
  'src/render/text_field.cpp',
  'src/render/panel_chrome.cpp',
  'src/render/marquee_text.cpp',
  'src/render/animated_image.cpp',
  'src/render/overlay_panel.cpp',
  'src/render/toplevel_window.cpp',
  'src/render/popup_window.cpp',
  'src/render/layer_surface.cpp',
  'src/render/slider.cpp',
  'src/render/arc_gauge.cpp',
  'src/render/progress_bar.cpp',
  'src/render/dock_row.cpp',
  'src/render/video_texture.cpp',
]

adastria_shell_service_test_sources = [
  'src/service/bluetooth_service.cpp',
  'src/service/brightness_service.cpp',
  'src/service/network_service.cpp',
  'src/service/tray_service.cpp',
  'src/service/upower_service.cpp',
  'src/service/mpris_service.cpp',
  'src/service/telemetry_service.cpp',
  'src/service/input_service.cpp',
  'src/service/output_service.cpp',
  'src/service/wallpaper_service.cpp',
  'src/service/media_service.cpp',
  'src/service/settings_service.cpp',
  'src/service/icon_service.cpp',
  'src/service/dock_service.cpp',
]

adastria_shell_service_main_only_sources = [
  'src/service/pipewire_service.cpp',
  'src/service/frame_service.cpp',
  'src/service/hyprland_service.cpp',
  'src/service/capture_service.cpp',
  'src/service/text_input_service.cpp',
  'src/service/notification_service.cpp',
  'src/service/polkit_service.cpp',
]

adastria_shell_launcher_main_only_sources = [
  'src/modules/launcher.cpp',
]

adastria_shell_launcher_test_sources = [
  'src/modules/launcher/apps_provider.cpp',
  'src/modules/launcher/desktop_entry.cpp',
  'src/modules/launcher/files_provider.cpp',
  'src/modules/launcher/launch_action.cpp',
  'src/modules/launcher/search.cpp',
  'src/modules/launcher/submenu.cpp',
  'src/modules/launcher/visit_store.cpp',
]

adastria_shell_bar_test_sources = []

adastria_shell_bar_panels_main_only_sources = [
  'src/modules/bar/panel/tray_panel.cpp',
  'src/modules/bar/panel/network_panel.cpp',
  'src/modules/bar/panel/bluetooth_panel.cpp',
  'src/modules/bar/panel/volume_panel.cpp',
  'src/modules/bar/panel/battery_panel.cpp',
  'src/modules/bar/panel/system_monitor_panel.cpp',
  'src/modules/bar/panel/clock_panel.cpp',
]

adastria_shell_bar_main_only_sources = [
  'src/modules/bar.cpp',
  'src/modules/bar/widget/widget_capsule.cpp',
  'src/modules/bar/widget/clock_widget.cpp',
  'src/modules/bar/widget/workspace_widget.cpp',
  'src/modules/bar/widget/battery_widget.cpp',
  'src/modules/bar/widget/logout_widget.cpp',
  'src/modules/bar/widget/bluetooth_widget.cpp',
  'src/modules/bar/widget/network_widget.cpp',
  'src/modules/bar/widget/dashboard_widget.cpp',
  'src/modules/bar/widget/dock_widget.cpp',
  'src/modules/bar/widget/system_monitor_widget.cpp',
  'src/modules/bar/widget/tray_widget.cpp',
  'src/modules/bar/widget/volume_widget.cpp',
]

adastria_shell_idle_main_only_sources = [
  'src/modules/idle.cpp',
]

adastria_shell_lock_test_sources = [
  'src/modules/lock/layout.cpp',
]

adastria_shell_lock_main_only_sources = [
  'src/modules/lock.cpp',
  'src/modules/lock/pam_authenticator.cpp',
]

adastria_shell_overlay_main_only_sources = [
  'src/modules/dock.cpp',
  'src/modules/osd.cpp',
  'src/modules/wallpaper.cpp',
  'src/modules/notification.cpp',
  'src/modules/logout.cpp',
  'src/modules/dashboard.cpp',
  'src/modules/overview.cpp',
  'src/modules/rain.cpp',
  'src/modules/rain/matrix_rain.cpp',
  'src/modules/rain/stiletto_rain.cpp',
  'src/modules/visualizer.cpp',
  'src/modules/visualizer/audio_capture.cpp',
  'src/modules/visualizer/audio_stages.cpp',
  'src/modules/visualizer/bar_visualizer.cpp',
  'src/modules/visualizer/sphere_visualizer.cpp',
  'src/modules/visualizer/visualizer_shaders.cpp',
  'src/modules/polkit.cpp',
]

adastria_shell_visualizer_test_sources = [
  'src/modules/visualizer/fft.cpp',
]

adastria_shell_app_test_sources = [
  'src/app/config.cpp',
]

adastria_shell_app_main_only_sources = [
  'src/app/single_instance_lock.cpp',
  'src/app/ipc.cpp',
  'src/app/key_dispatch.cpp',
  'src/app/module_registry.cpp',
  'src/app/monitor_output.cpp',
  'src/app/service_registry.cpp',
  'src/app/user_info.cpp',
  'src/app/wayland_registry.cpp',
]

adastria_shell_settings_main_only_sources = [
  'src/modules/settings.cpp',
  'src/modules/settings/wallpaper_tab.cpp',
  'src/modules/settings/displays_tab.cpp',
  'src/modules/settings/idle_tab.cpp',
  'src/modules/settings/logout_tab.cpp',
  'src/modules/settings/visualizer_tab.cpp',
  'src/modules/settings/rain_tab.cpp',
  'src/modules/settings/animation_tab.cpp',
]

adastria_shell_split_sources = [
  adastria_shell_core_split_sources,
  adastria_shell_render_test_sources,
  adastria_shell_render_main_only_sources,
  adastria_shell_service_test_sources,
  adastria_shell_service_main_only_sources,
  adastria_shell_launcher_test_sources,
  adastria_shell_launcher_main_only_sources,
  adastria_shell_idle_main_only_sources,
  adastria_shell_lock_test_sources,
  adastria_shell_lock_main_only_sources,
  adastria_shell_overlay_main_only_sources,
  adastria_shell_visualizer_test_sources,
  adastria_shell_app_test_sources,
  adastria_shell_app_main_only_sources,
  adastria_shell_settings_main_only_sources,
  adastria_shell_bar_test_sources,
  adastria_shell_bar_panels_main_only_sources,
  adastria_shell_bar_main_only_sources,
]

adastria_shell_test_split_sources = [
  adastria_shell_core_split_sources,
  adastria_shell_render_test_sources,
  adastria_shell_service_test_sources,
  adastria_shell_launcher_test_sources,
  adastria_shell_lock_test_sources,
  adastria_shell_bar_test_sources,
  adastria_shell_visualizer_test_sources,
  adastria_shell_app_test_sources,
]

# ffmpeg's SONAME isn't stable across releases (see readme.md), so the only
# code that touches it is a dlopen'd plugin: a missing/mismatched ffmpeg then
# only disables animated content (loader falls back gracefully) instead of
# blocking startup.
media_plugin = shared_module(
  'adastria-shell-media',
  'src/service/media_plugin.cpp',
  'src/core/log.cpp', # klog() isn't exported by the host (no -rdynamic); the plugin needs its own copy
  include_directories: adastria_shell_inc,
  dependencies: [
    libavformat_dep,
    libavcodec_dep,
    libavfilter_dep,
    libavutil_dep,
    libdrm_dep,
  ],
  install: true,
  install_dir: adastria_shell_data_dir / 'plugins',
)

executable(
  'adastria',
  'src/adastria-shell.cpp',
  adastria_shell_split_sources,
  proto_gen_all,
  include_directories: adastria_shell_inc,
  dependencies: [
    wayland_client_dep,
    wayland_egl_dep,
    egl_dep,
    glesv2_dep,
    cairo_dep,
    pangocairo_dep,
    nlohmann_json_dep,
    sdbus_cpp_dep,
    freetype2_dep,
    libpipewire_dep,
    xkbcommon_dep,
    threads_dep,
    libpng_dep,
    libjpeg_dep,
    librsvg_dep,
    libdrm_dep,
    pam_dep,
    glib_dep,
    gio_dep,
    gobject_dep,
    polkit_gobject_dep,
    polkit_agent_dep,
  ],
  cpp_args: [
    '-DADASTRIA_SHELL_FONT_DIR="' + adastria_shell_font_dir + '"',
    '-DADASTRIA_SHELL_SHADER_DIR="' + adastria_shell_data_dir / 'shaders' + '"',
    '-DADASTRIA_SHELL_DEFAULT_WALLPAPER="' + adastria_shell_data_dir / 'stellar-restoration.png' + '"',
    '-DADASTRIA_SHELL_BULLET_DIR="' + adastria_shell_bullet_dir + '"',
    '-DADASTRIA_SHELL_YUJIMAI_FONT="' + adastria_shell_font_dir / 'YujiMai.ttf' + '"',
    '-DADASTRIA_SHELL_LOGOUT_LOGO="' + adastria_shell_data_dir / 'logout/logo.gif' + '"',
    '-DADASTRIA_SHELL_LOGOUT_LOGO_STATIC="' + adastria_shell_data_dir / 'logout/logo.png' + '"',
    '-DADASTRIA_SHELL_IDLE_LOGO="' + adastria_shell_data_dir / 'stellar-restoration.svg' + '"',
    '-DADASTRIA_SHELL_STILETTO_SPRITE="' + adastria_shell_data_dir / 'stiletto.svg' + '"',
    '-DADASTRIA_SHELL_INPUT_ECHO="' + adastria_shell_data_dir / 'electro.png' + '"',
    '-DADASTRIA_SHELL_PAM_DIR="' + adastria_shell_data_dir / 'pam' + '"',
    '-DADASTRIA_SHELL_PROFILE_MEDIA="' + adastria_shell_data_dir / 'gifs/profile.gif' + '"',
    '-DADASTRIA_SHELL_MEDIA_PLUGIN="' + adastria_shell_data_dir / 'plugins/libadastria-shell-media.so' + '"',
  ],
  install: true,
  install_dir: 'bin',
)

adastria_shell_test = executable(
  'adastria-shell-test',
  'test/adastria-shell-test.cpp',
  adastria_shell_test_split_sources,
  proto_gen_test,
  'test/app/test_config.cpp',
  'test/app/test_wallpaper_resolve.cpp',
  'test/core/test_async_process.cpp',
  'test/core/test_deferred_call.cpp',
  'test/core/test_path_home.cpp',
  'test/core/test_poll_source.cpp',
  'test/dbus/test_network_parse.cpp',
  'test/dbus/test_bluetooth.cpp',
  'test/dbus/test_mpris.cpp',
  'test/launcher/test_launcher.cpp',
  'test/wayland/test_keyboard.cpp',
  'test/wayland/test_active_output.cpp',
  'test/wayland/test_dock.cpp',
  'test/system/test_rfkill.cpp',
  'test/system/test_cpu_temp.cpp',
  'test/system/test_gpu_temp.cpp',
  'test/system/test_system_stats.cpp',
  'test/render/test_animation.cpp',
  'test/render/test_animated_image.cpp',
  'test/render/test_marquee_scroll.cpp',
  'test/render/test_palette.cpp',
  'test/render/test_image_decode.cpp',
  'test/render/test_text_elide.cpp',
  'test/lock/test_layout.cpp',
  'test/visualizer/test_fft.cpp',
  include_directories: adastria_shell_inc,
  dependencies: [
    threads_dep,
    xkbcommon_dep,
    wayland_client_dep,
    wayland_egl_dep,
    egl_dep,
    cairo_dep,
    pangocairo_dep,
    freetype2_dep,
    sdbus_cpp_dep,
    libpng_dep,
    libjpeg_dep,
    librsvg_dep,
    glesv2_dep,
    nlohmann_json_dep,
  ],
  cpp_args: [
    # Source asset, not the install dir, so the test runs uninstalled.
    '-DADASTRIA_SHELL_DEFAULT_WALLPAPER="' + meson.current_source_dir() / 'assets/stellar-restoration.png' + '"',
    '-DADASTRIA_SHELL_BULLET_DIR="' + adastria_shell_bullet_dir + '"',
    '-DADASTRIA_SHELL_FONT_DIR="' + adastria_shell_font_dir + '"',
    '-DADASTRIA_SHELL_MEDIA_PLUGIN="' + adastria_shell_data_dir / 'plugins/libadastria-shell-media.so' + '"',
  ],
)

test('adastria-shell', adastria_shell_test)
