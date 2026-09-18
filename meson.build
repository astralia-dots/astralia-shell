project(
  'astralia-shell',
  'c',
  'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++23', 'warning_level=3'],
)

# =======
# WAYLAND
# =======
wayland_backend_opt = get_option('wayland_backend')
wayland_client_dep  = dependency('wayland-client', required: wayland_backend_opt)
wayland_egl_dep     = dependency('wayland-egl', required: wayland_backend_opt)

wayland_deps    = [wayland_client_dep, wayland_egl_dep]
wayland_enabled = wayland_client_dep.found() and wayland_egl_dep.found()

# ===
# X11
# ===
x11_backend_opt   = get_option('x11_backend')
x11_dep           = dependency('x11', required: x11_backend_opt)
x11_xcb_dep       = dependency('x11-xcb', required: x11_backend_opt)
xcb_dep           = dependency('xcb', required: x11_backend_opt)
xi_dep            = dependency('xi', required: x11_backend_opt)
xcomposite_dep    = dependency('xcomposite', required: x11_backend_opt)
xkbcommon_x11_dep = dependency('xkbcommon-x11', required: x11_backend_opt)
xcursor_dep       = dependency('xcursor', required: x11_backend_opt)
xrandr_dep        = dependency('xrandr', required: x11_backend_opt)
xext_dep          = dependency('xext', required: x11_backend_opt) # SHAPE extension, click-through input regions

x11_deps    = [x11_dep, x11_xcb_dep, xcb_dep, xi_dep, xcomposite_dep, xkbcommon_x11_dep, xcursor_dep, xrandr_dep, xext_dep]
x11_enabled = x11_dep.found() and x11_xcb_dep.found() and xcb_dep.found() and xi_dep.found() and xcomposite_dep.found() and xkbcommon_x11_dep.found() and xcursor_dep.found() and xrandr_dep.found() and xext_dep.found()

# ===================
# BACKEND VALIDATION
# ===================
if not wayland_enabled and not x11_enabled
  error('astralia-shell: neither the Wayland backend nor the X11 backend could be configured; install the Wayland (wayland-client, wayland-egl) or X11 (x11, x11-xcb, xcb, xi, xcomposite, xkbcommon-x11, xcursor, xrandr, xext) development packages')
endif

astralia_shell_backend_defines = []
if wayland_enabled
  astralia_shell_backend_defines += '-DASTRALIA_HAVE_WAYLAND'
endif
if x11_enabled
  astralia_shell_backend_defines += '-DASTRALIA_HAVE_X11'
endif

# ============
# DEPENDENCIES
# ============
egl_dep            = dependency('egl')
glesv2_dep         = dependency('glesv2') # ES 2.0 entry points; the shell targets #version 100
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
libdrm_dep         = dependency('libdrm') # DRM_FORMAT_* fourccs for zero-copy VAAPI dma-buf import
glib_dep           = dependency('glib-2.0')
gio_dep            = dependency('gio-2.0')
gobject_dep        = dependency('gobject-2.0')
polkit_gobject_dep = dependency('polkit-gobject-1')
polkit_agent_dep   = dependency('polkit-agent-1')

common_deps = [
  egl_dep, glesv2_dep, cairo_dep, pangocairo_dep, nlohmann_json_dep, sdbus_cpp_dep,
  freetype2_dep, libpipewire_dep, xkbcommon_dep, pam_dep, libpng_dep, libjpeg_dep,
  librsvg_dep, threads_dep, libavformat_dep, libavcodec_dep, libavfilter_dep,
  libavutil_dep, libdrm_dep, glib_dep, gio_dep, gobject_dep, polkit_gobject_dep,
  polkit_agent_dep,
]

astralia_shell_inc = include_directories('src')

# ======
# ASSETS
# ======
astralia_shell_data_dir   = get_option('prefix') / get_option('datadir') / 'astralia-shell'
astralia_shell_font_dir   = astralia_shell_data_dir / 'fonts'
astralia_shell_bullet_dir = astralia_shell_data_dir / 'constellation'

astralia_shell_flat_assets = [
  ['assets/fonts/tabler-icons.ttf', astralia_shell_font_dir],
  ['assets/fonts/YujiMai.ttf', astralia_shell_font_dir],
  ['assets/stellar-restoration.png', astralia_shell_data_dir],
  ['assets/stellar-restoration.svg', astralia_shell_data_dir], # idle screensaver logo (placeholder)
  ['assets/stiletto.svg', astralia_shell_data_dir],
  ['assets/electro.png', astralia_shell_data_dir],
  ['assets/gifs/profile.gif', astralia_shell_data_dir / 'gifs'],
  ['assets/logout/logo.gif', astralia_shell_data_dir / 'logout'],
  ['assets/logout/logo.png', astralia_shell_data_dir / 'logout'],
  ['assets/pam/astralia-shell', astralia_shell_data_dir / 'pam'],
]
foreach a : astralia_shell_flat_assets
  install_data(a[0], install_dir: a[1])
endforeach
install_subdir('assets/shaders', install_dir: astralia_shell_data_dir)

foreach n : ['1', '2', '3', '4', '5', '6'] # keqing constellation bullets
  install_data('assets/constellation/C' + n + '.png', install_dir: astralia_shell_bullet_dir)
endforeach

# ==================
# WAYLAND PROTOCOLS
# ==================
wayland_scanner = find_program('wayland-scanner')
wl_proto_dir = dependency('wayland-protocols').get_variable(pkgconfig: 'pkgdatadir')

# name => [xml, needs_sed]. The sed fixup renames wlr-layer-shell's 'namespace'
# arg (a C++ keyword), matching noctalia's fix. xdg-shell, tablet-unstable-v2,
# and wlr-foreign-toplevel-management are listed only so types their requests
# reference still link; nothing here calls them directly.
protocol_defs = [
  ['xdg-shell', wl_proto_dir / 'stable/xdg-shell/xdg-shell.xml', false],
  ['wlr-layer-shell-unstable-v1', 'protocols/wlr-layer-shell-unstable-v1.xml', true],
  ['ext-idle-notify-v1', wl_proto_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml', false],
  ['cursor-shape-v1', wl_proto_dir / 'staging/cursor-shape/cursor-shape-v1.xml', false],
  ['tablet-unstable-v2', wl_proto_dir / 'unstable/tablet/tablet-unstable-v2.xml', false],
  ['hyprland-toplevel-export-v1', 'protocols/hyprland-toplevel-export-v1.xml', false],
  ['wlr-foreign-toplevel-management-unstable-v1', 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml', false],
  ['text-input-unstable-v3', 'protocols/text-input-unstable-v3.xml', false],
  ['ext-session-lock-v1', wl_proto_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml', false],
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
if wayland_enabled
  foreach _name, tgts : proto
    proto_gen_all += tgts
  endforeach
endif

# Test binary links only the protocols its sources actually reference.
proto_gen_test = [
  proto['wlr-layer-shell-unstable-v1'],
  proto['xdg-shell'],
  proto['cursor-shape-v1'],
  proto['tablet-unstable-v2'],
]

# =======
# SOURCES
# =======
# .h/.cpp module pairs shared by the main executable and the test binary;
# header-only .hpp modules need no entry. *_test_sources are exercised by
# test/**; *_main_only_sources pull in EGL/GL/cairo/pango/pipewire or
# generated protocol code and have no test coverage.

astralia_shell_core_split_sources = [
  'src/core/log.cpp',
  'src/core/poll_source.cpp',
  'src/core/deferred_call.cpp',
  'src/core/async_process.cpp',
  'src/core/path_home.cpp',
]

astralia_shell_render_test_sources = [
  'src/render/animation.cpp',
  'src/render/marquee_scroll.cpp',
  'src/render/texture.cpp',
  'src/render/image.cpp',
  'src/render/text_elide.cpp',
]

astralia_shell_render_main_only_sources = [
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
  'src/render/egl_surface.cpp',
]

astralia_shell_service_test_sources = [
  'src/service/bluetooth_service.cpp',
  'src/service/brightness_service.cpp',
  'src/service/network_service.cpp',
  'src/service/tray_service.cpp',
  'src/service/upower_service.cpp',
  'src/service/mpris_service.cpp',
  'src/service/telemetry_service.cpp',
  'src/service/input_service.cpp',
  'src/service/wallpaper_service.cpp',
  'src/service/media_service.cpp',
  'src/service/settings_service.cpp',
  'src/service/icon_service.cpp',
  'src/service/dock_service.cpp',
]

astralia_shell_service_main_only_sources = [
  'src/service/output_service.cpp',
  'src/service/pipewire_service.cpp',
  'src/service/frame_service.cpp',
  'src/service/hyprland_service.cpp',
  'src/service/capture_service.cpp',
  'src/service/notification_service.cpp',
  'src/service/polkit_service.cpp',
]

astralia_shell_launcher_main_only_sources = [
  'src/modules/launcher.cpp',
]

astralia_shell_launcher_test_sources = [
  'src/modules/launcher/apps_provider.cpp',
  'src/modules/launcher/desktop_entry.cpp',
  'src/modules/launcher/files_provider.cpp',
  'src/modules/launcher/launch_action.cpp',
  'src/modules/launcher/search.cpp',
  'src/modules/launcher/submenu.cpp',
  'src/modules/launcher/visit_store.cpp',
]

astralia_shell_bar_test_sources = []

astralia_shell_bar_panels_main_only_sources = [
  'src/modules/bar/panel/tray_panel.cpp',
  'src/modules/bar/panel/network_panel.cpp',
  'src/modules/bar/panel/bluetooth_panel.cpp',
  'src/modules/bar/panel/volume_panel.cpp',
  'src/modules/bar/panel/battery_panel.cpp',
  'src/modules/bar/panel/system_monitor_panel.cpp',
  'src/modules/bar/panel/clock_panel.cpp',
]

astralia_shell_bar_main_only_sources = [
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

astralia_shell_idle_main_only_sources = [
  'src/modules/idle.cpp',
]

astralia_shell_lock_test_sources = [
  'src/modules/lock/layout.cpp',
]

astralia_shell_lock_main_only_sources = [
  'src/modules/lock.cpp',
  'src/modules/lock/pam_authenticator.cpp',
]

astralia_shell_overlay_main_only_sources = [
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

astralia_shell_visualizer_test_sources = [
  'src/modules/visualizer/fft.cpp',
]

astralia_shell_app_test_sources = [
  'src/app/config.cpp',
]

astralia_shell_app_main_only_sources = [
  'src/app/backend.cpp',
  'src/app/backend_bootstrap.cpp',
  'src/app/backend_poll.cpp',
  'src/app/input_dispatch.cpp',
  'src/app/single_instance_lock.cpp',
  'src/app/ipc.cpp',
  'src/app/key_dispatch.cpp',
  'src/app/module_registry.cpp',
  'src/app/monitor_output.cpp',
  'src/app/service_registry.cpp',
  'src/app/user_info.cpp',
  'src/app/wayland_registry.cpp',
]

astralia_shell_settings_main_only_sources = [
  'src/modules/settings.cpp',
  'src/modules/settings/wallpaper_tab.cpp',
  'src/modules/settings/displays_tab.cpp',
  'src/modules/settings/idle_tab.cpp',
  'src/modules/settings/logout_tab.cpp',
  'src/modules/settings/visualizer_tab.cpp',
  'src/modules/settings/rain_tab.cpp',
  'src/modules/settings/animation_tab.cpp',
]

# ===============
# WAYLAND SOURCES
# ===============
astralia_shell_wayland_sources = []
if wayland_enabled
  astralia_shell_wayland_sources = [
    'src/wayland/egl_surface.cpp',
    'src/wayland/layer_surface.cpp',
    'src/wayland/toplevel_window.cpp',
    'src/wayland/popup_window.cpp',
    'src/wayland/output_service.cpp',
    'src/wayland/frame_service.cpp',
    'src/wayland/capture_service.cpp',
    'src/wayland/session_lock.cpp',
    'src/wayland/input_service.cpp',
    'src/wayland/bootstrap.cpp',
    'src/wayland/text_input_service.cpp',
  ]
endif

# ===========
# X11 SOURCES
# ===========
astralia_shell_x11_sources = []
if x11_enabled
  astralia_shell_x11_sources = [
    'src/x11/egl_surface.cpp',
    'src/x11/layer_surface.cpp',
    'src/x11/atoms.cpp',
    'src/x11/toplevel_window.cpp',
    'src/x11/popup_window.cpp',
    'src/x11/output_service.cpp',
    'src/x11/frame_service.cpp',
    'src/x11/capture_service.cpp',
    'src/x11/session_lock.cpp',
    'src/x11/input_service.cpp',
    'src/x11/bootstrap.cpp',
    'src/x11/output_bootstrap.cpp',
  ]
  if not wayland_enabled
    astralia_shell_x11_sources += 'src/x11/text_input_service.cpp'
  endif
endif

astralia_shell_split_sources = [
  astralia_shell_core_split_sources,
  astralia_shell_wayland_sources,
  astralia_shell_x11_sources,
  astralia_shell_render_test_sources,
  astralia_shell_render_main_only_sources,
  astralia_shell_service_test_sources,
  astralia_shell_service_main_only_sources,
  astralia_shell_launcher_test_sources,
  astralia_shell_launcher_main_only_sources,
  astralia_shell_idle_main_only_sources,
  astralia_shell_lock_test_sources,
  astralia_shell_lock_main_only_sources,
  astralia_shell_overlay_main_only_sources,
  astralia_shell_visualizer_test_sources,
  astralia_shell_app_test_sources,
  astralia_shell_app_main_only_sources,
  astralia_shell_settings_main_only_sources,
  astralia_shell_bar_test_sources,
  astralia_shell_bar_panels_main_only_sources,
  astralia_shell_bar_main_only_sources,
]

astralia_shell_test_split_sources = [
  astralia_shell_core_split_sources,
  astralia_shell_render_test_sources,
  astralia_shell_service_test_sources,
  astralia_shell_launcher_test_sources,
  astralia_shell_lock_test_sources,
  astralia_shell_bar_test_sources,
  astralia_shell_visualizer_test_sources,
  astralia_shell_app_test_sources,
]

# ============
# MEDIA PLUGIN
# ============
# ffmpeg's SONAME isn't stable across releases (see readme.md); the plugin is
# dlopen'd so a missing/mismatched ffmpeg only disables animated content
# instead of blocking startup.
media_plugin = shared_module(
  'astralia-shell-media',
  'src/service/media_plugin.cpp',
  'src/core/log.cpp', # klog() isn't exported by the host (no -rdynamic); needs its own copy
  include_directories: astralia_shell_inc,
  dependencies: [libavformat_dep, libavcodec_dep, libavfilter_dep, libavutil_dep, libdrm_dep],
  install: true,
  install_dir: astralia_shell_data_dir / 'plugins',
)

# ===============
# MAIN EXECUTABLE
# ===============
executable(
  'astralia',
  'src/astralia-shell.cpp',
  astralia_shell_split_sources,
  proto_gen_all,
  include_directories: astralia_shell_inc,
  dependencies: common_deps + wayland_deps + x11_deps,
  cpp_args: astralia_shell_backend_defines + [
    '-DASTRALIA_SHELL_FONT_DIR="' + astralia_shell_font_dir + '"',
    '-DASTRALIA_SHELL_SHADER_DIR="' + astralia_shell_data_dir / 'shaders' + '"',
    '-DASTRALIA_SHELL_DEFAULT_WALLPAPER="' + astralia_shell_data_dir / 'stellar-restoration.png' + '"',
    '-DASTRALIA_SHELL_BULLET_DIR="' + astralia_shell_bullet_dir + '"',
    '-DASTRALIA_SHELL_YUJIMAI_FONT="' + astralia_shell_font_dir / 'YujiMai.ttf' + '"',
    '-DASTRALIA_SHELL_LOGOUT_LOGO="' + astralia_shell_data_dir / 'logout/logo.gif' + '"',
    '-DASTRALIA_SHELL_LOGOUT_LOGO_STATIC="' + astralia_shell_data_dir / 'logout/logo.png' + '"',
    '-DASTRALIA_SHELL_IDLE_LOGO="' + astralia_shell_data_dir / 'stellar-restoration.svg' + '"',
    '-DASTRALIA_SHELL_STILETTO_SPRITE="' + astralia_shell_data_dir / 'stiletto.svg' + '"',
    '-DASTRALIA_SHELL_INPUT_ECHO="' + astralia_shell_data_dir / 'electro.png' + '"',
    '-DASTRALIA_SHELL_PAM_DIR="' + astralia_shell_data_dir / 'pam' + '"',
    '-DASTRALIA_SHELL_PROFILE_MEDIA="' + astralia_shell_data_dir / 'gifs/profile.gif' + '"',
    '-DASTRALIA_SHELL_MEDIA_PLUGIN="' + astralia_shell_data_dir / 'plugins/libastralia-shell-media.so' + '"',
  ],
  install: true,
  install_dir: 'bin',
)

# ===============
# TEST EXECUTABLE
# ===============
if wayland_enabled
  test_deps = [
    threads_dep, xkbcommon_dep, egl_dep, cairo_dep, pangocairo_dep, freetype2_dep,
    sdbus_cpp_dep, libpng_dep, libjpeg_dep, librsvg_dep, glesv2_dep, nlohmann_json_dep,
  ] + wayland_deps

  astralia_shell_test_only_sources = [
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
  ]

  astralia_shell_test = executable(
    'astralia-shell-test',
    'test/astralia-shell-test.cpp',
    astralia_shell_test_split_sources,
    astralia_shell_test_only_sources,
    proto_gen_test,
    include_directories: astralia_shell_inc,
    dependencies: test_deps,
    cpp_args: [
      '-DASTRALIA_SHELL_DEFAULT_WALLPAPER="' + meson.current_source_dir() / 'assets/stellar-restoration.png' + '"', # source asset, so the test runs uninstalled
      '-DASTRALIA_SHELL_BULLET_DIR="' + astralia_shell_bullet_dir + '"',
      '-DASTRALIA_SHELL_FONT_DIR="' + astralia_shell_font_dir + '"',
      '-DASTRALIA_SHELL_MEDIA_PLUGIN="' + astralia_shell_data_dir / 'plugins/libastralia-shell-media.so' + '"',
    ],
  )

  test('astralia-shell', astralia_shell_test)
endif
