# `astralia-shell` index

## Rule

- One-line, no break.
- Grouped by `directory`, one `##` heading per directory.
- Entry format: `file`: Purpose (≤ 20 words).
- Reflect current structure and function of each file in the code base.
- No mentions of past fixes.

## src/app

- `backend.h`+`.cpp`: `backend_connect()` probes Wayland then X11 and caches the live connection; `active_backend()`/`active_display()` let every `src/wayland/`+`src/x11/` seam query which backend and connection are live without re-plumbing it through every call; `backend_wait_dispatch()` is the narrow blocking helper used by every `while (!configured) ...` wait loop.
- `backend_bootstrap.h`+`.cpp`: Dispatches the one-time startup handshake to `wayland::bootstrap`/`x11::bootstrap` - registry bind+roundtrips on Wayland, `XRandR` output enumeration plus direct `keyboard_attach_seat`/`pointer_bind` calls on X11.
- `backend_poll.h`+`.cpp`: Main-loop poll-fd/flush/dispatch dispatch; the X11 branch drains `XPending`/`XNextEvent`, routing `GenericEvent`/`XInput2` to `x11::handle_xi_device_event`, `ConfigureNotify` to `x11::toplevel_window_handle_configure_notify`, and `ClientMessage` (`WM_DELETE_WINDOW`) to `x11::toplevel_window_handle_client_message`.
- `input_dispatch.cpp`: Backend dispatch for `keyboard_attach_seat`/`pointer_bind`/`pointer_release`/`pointer_set_cursor_shape` (declared in `service/input_service.h`); kept out of `service/input_service.cpp` so that file stays test-linked and free of Wayland/X11 headers.
- `config.h`+`.cpp`: JSON config loader/saver with atomic write and inotify hot-reload.
- `single_instance_lock.h`+`.cpp`: `flock()`-based single-instance lock.
- `ipc.h`+`.cpp`: Astralia Shell's own control socket, client/server request handling; verb table from each module.
- `key_dispatch.h`+`.cpp`: Routes key events to whichever module owns the surface `KeyboardState::focused_surface` currently names, so `astralia-shell.cpp` never names a module's key handler.
- `monitor_output.h`+`.cpp`: `MonitorOutput` per-output state, monitor create/activate/destroy lifecycle, config-apply orchestration, settings retarget.
- `module.h`: `Module` interface: per-surface overlay boundary, default no-op virtuals, plus `apply_config` and `on_output_removed` hooks.
- `per_monitor_module.h`: `PerMonitorModule` interface, the per-surface per-monitor boundary; default no-op virtuals, unnamed params.
- `module_registry.h`+`.cpp`: `build_app_modules`/`build_per_monitor_modules` composition root; also bridges `app/` code to the lock module without a module include.
- `wayland_registry.h`+`.cpp`: Backend-agnostic bootstrap leftovers - `bar_layer_surface_configure`, and `bootstrap_egl`/`renderer_bootstrap_init` (the `eglGetDisplay`/`EGLContext`/GL-string-log sequence, identical for either backend since `EGLNativeDisplayType` is just the raw connection pointer). The actual `wl_registry` bind/listener wiring lives in `src/wayland/bootstrap.cpp`.
- `wayland_state.h`: `WaylandState`, forward-declared Wayland pointer fields and every process-wide service's owned state; forward-declares `MonitorOutput`. Concrete Wayland/X11 types never leak through this header.
- `service.h`: `Service` interface, the process-wide boundary for cross-cutting services: `init`/`timer_tick`/`poll_sources`.
- `service_registry.h`+`.cpp`: `build_services` composition root, one `Service` subclass per cross-cutting service.
- `user_info.h`+`.cpp`: `getpwuid`-based username, `/etc/os-release` `PRETTY_NAME`, `sysinfo`-based uptime string, and `profile_media_path` resolution, shared across modules.
- `text_input_client.h`: `TextInputClient` interface, `TextInputState`/`TextInputEdit`, implemented by each field-owning module's wrapper class.

## src/config

- `bar_config.h`: Bar geometry, spacing, and pill-order constants.
- `dock_config.h`: Dock capsule geometry, icon size/spacing, focused/unfocused icon opacity, reorder timing, bottom margin, autohide peek/reveal/hide constants, animation-owner bases.
- `launcher_config.h`: Every launcher data type and constant, no function bodies.
- `osd_config.h`: OSD surface size/margin/duration/animation-owner constants.
- `notification_config.h`: Notification card padding/size/timing constants.
- `logout_config.h`: Ring-menu geometry, entry/exit hold/slash/burst/implode timings, `thunder_burst` constants, `{8/3}` star step, animation owner ids, and the 8-button action table.
- `dashboard_config.h`: Dashboard card-stack geometry and gauge/temp-warn color constants.
- `overview_config.h`: Overview workspace-grid geometry, timing, and live-capture throttle constants.
- `wallpaper_config.h`: Wallpaper layer-shell namespace constant, `WallpaperTransition` enum (`None`/`Fade`/`Wipe`/`Disc`/`Stripes`/`Zoom`/`Honeycomb`/`Random`), and the fixed cross-transition duration/edge-smoothness constants.
- `settings_config.h`: Settings panel layout/animation constants, `SettingsFieldId` enum, `SettingsTabDef` type, and the six nav-rail tab labels.
- `rain_config.h`: `RainMode`/`RainParams` types, shared rain window/timing constants, plus per-sim `kMatrixRain*` and `kStilettoRain*` tuning constants.
- `idle_config.h`: Idle recent-activity pulse and idle-overlay fade, logo-speed, and layer-namespace constants.
- `lock_config.h`: Lock-screen card ratio, three-column and side-panel geometry, fetch/media/resources/notification-dock constants, dot/input/avatar sizes, entrance/exit animation timings, and per-property animation owner ids.
- `visualizer_config.h`: Audio visualizer canvas/capture/FFT/GPU-transform constants, bar geometry, and `VisualizerParams` runtime knobs with their clamp ranges.

## src/render

- `palette.h`: `Color` struct, compile-time hex parser, and the full ported color/metrics palette.
- `color_ops.h`: Runtime `with_alpha`/`lerp_color` color operations for animations.
- `texture.h`+`.cpp`: RAII GL texture handle and creation/update helpers, RGBA or direct BGRA upload, plus `GL_EXTENSIONS` cap probes.
- `panel_scroll.h`+`.cpp`: Shared scroll-offset/clamp/wheel-input helper for scrollable panel lists.
- `text.h`+`.cpp`: Cairo+Pango text rasterization at fixed sizes, plus one-off pixel sizing, fixed monospace advance, and string-to-`Texture` helpers.
- `text_elide.h`+`.cpp`: Pango-free character-count string elision, end (`elide`) and middle (`elide_middle`); linked into the test binary.
- `marquee_scroll.h`+`.cpp`: `MarqueeTextState` and the pure pause/scroll/pause/snap loop state machine, driven by `AnimationManager`; no Node/texture dependency.
- `marquee_text.h`+`.cpp`: `draw_marquee_text`, the Node-drawing wrapper around `marquee_scroll.h`; animates a clipped scroll only when text overflows.
- `animated_image.h`+`.cpp`: `AnimatedImage` playable still/animated picture; wall-clock frame cycling over the `media_service` `.rgba` cache, `show`/`hide` releasing frame textures while off-screen, ring+circular-crop draw.
- `renderer.h`+`.cpp`: GL draw calls, clip-stack and transform-stack management, shared across every surface; `draw_custom` runs a module-owned shader over the shared quad.
- `rect.h`: Shared `Rect{x,y,w,h}` struct for hit-testing.
- `panel_chrome.h`+`.cpp`: Shared box/header/confirm chrome, click-kind enum, `panel_region_hit`, `panel_draw_toggle_switch`, `panel_draw_centered_text`, and `panel_measure_row_actions`/`panel_draw_row_actions` (connect/forget pill or busy label) for on-demand panels.
- `node.h`+`.cpp`: `Node` retained-allocation scene graph with per-frame node pooling; kinds are rect/rounded-rect/texture/rounded-texture/video-texture/group; per-node `rotation`/`scale` about the node centre.
- `video_texture.h`+`.cpp`: `VideoTexture` RAII `EGLImageKHR`/`GL` handle plus `DrmFrameImport` dma-buf import for zero-copy `VAAPI` playback, and the `EGL_EXT_image_dma_buf_import` cap probe.
- `gl.h`+`.cpp`: Labelled shader compile/link helpers, reading `assets/shaders/` with an installed-then-dev-tree fallback, plus a `glGetError`-draining `gl_check`.
- `egl_surface.h`+`.cpp`: `NativeSurfaceHandle`/`NativeEglWindowHandle` opaque types and the `egl_native_window_*`/`native_surface_*` backend dispatch; bodies live in `src/wayland/`+`src/x11/`.
- `overlay_panel.h`+`.cpp`: Shared full-screen on-demand overlay surface: position-lock-on-toggle, live-height roll-down/collapse, and output-unplug surface release; layer-surface/EGL/commit calls all go through the backend-dispatched seams.
- `toplevel_window.h`+`.cpp`: Shared real-window surface lifecycle for compositor-managed windows (`xdg_toplevel` on Wayland, a WM-managed `Window` on X11); backend dispatch, bodies in `src/wayland/`+`src/x11/`.
- `popup_window.h`+`.cpp`: Shared positioned-popup surface lifecycle (`xdg_popup` parented via `zwlr_layer_surface_v1::get_popup` on Wayland, an override-redirect `Window` on X11); backend dispatch, bodies in `src/wayland/`+`src/x11/`.
- `layer_surface.h`+`.cpp`: `LayerSurfaceConfig`/`LayerSurfaceHandle` backend dispatch for panel surfaces (`zwlr_layer_surface_v1` on Wayland, an EWMH-struts `Window` on X11); bodies in `src/wayland/`+`src/x11/`.
- `scene.h`: Thin `Scene` holder over `node.h` - a root `Node` plus `dirty`/`draw`/`rebuild` one-liners; no scene-graph logic of its own.
- `image.h`+`.cpp`: JPEG/PNG/SVG decode (sniffed from content) and GL texture upload, no GIF; SVG rasterized via `librsvg`+Cairo; `load_image_texture_first_existing` picks the first candidate path that exists.
- `texture_cache.h`+`.cpp`: Path-keyed decoded-texture cache built on `texture.h`.
- `icon.h`+`.cpp`: Direct FreeType+Cairo rendering of single icon glyphs, plus `make_icon_texture` glyph-to-`Texture`.
- `icons.h`: Tabler Icons codepoint constants.
- `text_field.h`+`.cpp`: Shared single-line editable text buffer core, plus every shell input's shared caret, per-character pop, and row-slide animations.
- `animation.h`+`.cpp`: `AnimationManager`, wall-clock tween/easing engine, owner-tag auto-cancel.
- `slider.h`+`.cpp`: `draw_slider_track`, shared track+fill+click-region drawing for any slider.
- `arc_gauge.h`+`.cpp`: Shared cached circular arc-gauge texture plus icon/value/sub-label layout; diameter, stroke, and colors are caller params.
- `progress_bar.h`+`.cpp`: Shared track+fill rounded-bar drawing with a caller-set minimum fill width; no click regions or panel dependency.
- `dock_row.h`+`.cpp`: Per-window-class icon-texture cache and the icon-row draw with focus opacity and reorder slide; shared by `dock` and `dock_widget`.

## src/service

- `bluetooth_service.h`+`.cpp`: BlueZ D-Bus client, device-classification logic, rfkill soft-block reader/clearer.
- `brightness_service.h`+`.cpp`: Backlight `sysfs` reader and `inotify` watch, plus `brightness_set` via a `brightnessctl` subprocess; shared by the OSD service and dashboard.
- `network_service.h`+`.cpp`: NetworkManager client (nmcli subprocesses + D-Bus) and pure output parsers.
- `notification_service.h`+`.cpp`: `org.freedesktop.Notifications` D-Bus server, `NotificationRecord` store, and expiry sweep; consumed by `notification`'s renderer and `lock`'s dock.
- `tray_service.h`+`.cpp`: StatusNotifierWatcher/host implementation and DBusMenu tree fetch.
- `mpris_service.h`+`.cpp`: Minimal MPRIS client, async player scan and selection policy, transport control methods.
- `upower_service.h`+`.cpp`: UPower D-Bus client; single display device for the bar's battery pill, plus full device enumeration for the battery panel.
- `pipewire_service.h`+`.cpp`: Direct libpipewire client for OSD volume/mic triggers and volume-panel writes; also `DraggedSlider`, tag-to-node-id resolution, and drag-to-volume application.
- `telemetry_service.h`+`.cpp`: CPU/GPU temperature and usage via hwmon/thermal-zone/`nvidia-smi`, plus CPU frequency, CPU/RAM/disk usage, and network throughput.
- `frame_service.h`+`.cpp`: Frame-callback paint pacing shared across surfaces; first paint synchronous, later repaints deferred to `frame_done`.
- `input_service.h`+`.cpp`: `translate_key`/`keyboard_drain_events`/`keyboard_repeat_tick`/`pointer_drain_clicks`/`pointer_drain_scrolls` - the pure, backend-agnostic parts of `KeyboardState`/`PointerState` handling; test-linked. Seat/pointer-bind dispatch lives in `app/input_dispatch.cpp`, bodies in `src/wayland/`+`src/x11/` (`wl_seat`/`xkbcommon` vs `XInput2`/`xkbcommon-x11`/`Xcursor`).
- `text_input_service.h`: `TextInputService` class shell, fully forward-declared. Its body is backend-exclusive, not a runtime shim: `src/wayland/text_input_service.cpp` (the real `zwp_text_input_v3` glue, moved as-is) is compiled when `wayland_backend` is enabled, `src/x11/text_input_service.cpp` (all-methods-no-op stub; real `XIM` support is unimplemented) otherwise.
- `hyprland_service.h`+`.cpp`: Hyprland IPC client: per-monitor workspace/client state via request+event sockets, plus `hypr_tile_*` tiling actions dispatched as Lua calls.
- `capture_service.h`+`.cpp`: Per-window live capture backend dispatch (`hyprland-toplevel-export-v1`+`wl_shm` on Wayland, `XComposite`+`XGetImage` on X11); `toplevel_export_texture` is a pure lookup shared by both. Bodies in `src/wayland/`+`src/x11/`.
- `output_service.h`+`.cpp`: Pure-data `Output` struct plus `active_output_select` (inline, header-only, test-linked); `output_scale_watch`'s fractional-scale listener is backend dispatch, bodies in `src/wayland/`+`src/x11/` (X11's is a no-op - no live per-surface scale-change event on core X11).
- `wallpaper_service.h`+`.cpp`: Per-monitor, per-column wallpaper path/count/fill-mode resolution; a `bool animated` selects the static or animated config maps.
- `media_service.h`+`.cpp`: The shell's one media decoder, host side; loads `media_plugin` via `dlopen` and owns the async `.rgba` frame cache.
- `media_plugin.h`+`.cpp`: The `shared_module` linking `libavcodec`/`libavfilter`, isolated so a missing `ffmpeg` only disables animated content, not the whole shell.
- `settings_service.h`+`.cpp`: Settings field-text parsing into `Config` and the config-save wrapper.
- `icon_service.h`+`.cpp`: App icon path resolution across GTK icon themes; `resolve_window_icon_path` maps a window class to an icon via `.desktop` ids.
- `dock_service.h`+`.cpp`: `DockEntry` list for a monitor's active workspace from `HyprlandState`, sorted by window `x`, `focused` = `focus_history_id == 0`; pure, test-linked.
- `polkit_service.h`+`.cpp`: `PolkitAgent`, an in-session polkit authentication agent registering with `polkit-gobject-1`/`polkit-agent-1` and driving the session through its own nested `GMainContext`; `PolkitPollSource` bridges it into the shared poll loop.

## src/core

- `deferred_call.h`+`.cpp`: Cross-thread callback hand-off so worker threads can post to the main thread.
- `log.h`+`.cpp`: `klog()` dual stderr + logfile logging with timestamps; `klog_set_backend()` retargets the logfile to `astralia-wayland.log`/`astralia-x11.log` once the active backend is known; `klog_install_crash_handler()` also installs a `std::set_terminate` handler that logs the uncaught exception's `what()` before aborting.
- `path_home.h`+`.cpp`: `path_collapse_home`/`path_expand_home` `$HOME` <-> `~` path rewriters, shared by `config`, `settings`, and `launcher`.
- `poll_source.h`+`.cpp`: `PollSource` interface, `FnPollSource` helper, and `sdbus_poll_source` wrapping an sdbus connection's poll data.
- `async_process.h`+`.cpp`: Worker-thread subprocess runner, plus `spawn_detached` for fire-and-forget commands.

## src/modules

- `bar.h`+`.cpp`: Bar rendering, autohide geometry, pill-click dispatch, bar surface's own EGL; shared `WaylandState`-wide helpers.
- `dock.h`+`.cpp`: Per-monitor bottom layer-shell dock; centered capsule of the active workspace's window icons, own surface/EGL/scene, with optional autohide.
- `launcher.h`+`.cpp`: `LauncherState`, surface/EGL/tick/toggle/key/click/pointer-hover/paint core only.
- `osd.h`+`.cpp`: Volume/brightness popup, per-monitor, auto-hides, reactive to system state changes.
- `notification.h`+`.cpp`: Notification renderer; rebuilds render/animation state from `notification_service` records, per-monitor card paint, and per-monitor close-button dismissal.
- `logout.h`+`.cpp`: Logout ring overlay: entry/exit lightning-slash/shockwave choreography, animated centre logo, and its two custom shader effects.
- `dashboard.h`+`.cpp`: Dashboard singleton state: fixed top-right overlay, IPC/widget-triggered open, scrollable card layout, and brightness card.
- `overview.h`+`.cpp`: Overview state, Hyprland-only full-screen exclusive-keyboard overlay; paginated workspace grid, live per-window `hyprland-toplevel-export-v1` thumbnails, click/drag/keyboard focus-move-swap-close, IPC-only toggle.
- `wallpaper.h`+`.cpp`: Per-monitor wallpaper surface: static or animated columns per config, cross-transition on image change, shared by `lock` and idle ambient.
- `idle.h`+`.cpp`: Recent-activity idle clock feeding the per-monitor ambient/screensaver overlay surface; screensaver bounces an `AnimatedImage` logo, freed while not shown.
- `settings.h`+`.cpp`: Settings panel core: hosts per-tab modules, responsive nav rail, shared toggle widgets, and a separately-faded active-tab scene.
- `rain.h`+`.cpp`: Rain overlay, a real `xdg_toplevel` window; hosts the `MatrixRain`/`StilettoRain` sims and applies mode/speed config live.
- `visualizer.h`+`.cpp`: Audio visualizer overlay window; a dedicated self-pacing render thread draws either `SphereVisualizer` or `BarVisualizer`, fed by its own PipeWire capture.
- `lock.h`+`.cpp`: Session lock: `PAM` auth on a worker thread, three-column info card, `lock_paint` and password state machine all backend-agnostic. Surface acquire/create/destroy/release are backend dispatch (`session_lock_*`, bodies in `src/wayland/`+`src/x11/`); X11's stub always fails (`ext_session_lock_v1`-equivalent grab not yet implemented there).
- `polkit.h`+`.cpp`: Reactive singleton overlay prompting for the user's password on a polkit authentication request; centered card with `EaseOutBack`/`EaseInBack` scale-in/out, dot-masked password field shared with `lock`'s echo glyph.

## src/modules/visualizer

- `fft.h`+`.cpp`: Radix-2 DIT FFT, Hann window plus `log`/`fftScale`/`fftCutOff` magnitude tilt; `EGL`-free, linked into the test binary.
- `audio_capture.h`+`.cpp`: Own `pw_thread_loop` `11 kHz` stereo sink capture; `ncs` ring/fragment bookkeeping into `4096`-sample L/R buffers, `take()` snapshot under a mutex.
- `audio_stages.h`+`.cpp`: Render-thread GPU transform chain (peak-hold, decay, ring-averaged, frequency-smoothed) over `GL_R16` textures, for the left and right channels.
- `sphere_visualizer.h`+`.cpp`: `SphereVisualizer`: a multi-pass particle-accumulation/blob/glow GPU pipeline on a square canvas, rebuilt on resize, presented over a black backdrop.
- `bar_visualizer.h`+`.cpp`: `BarVisualizer`: a single-pass fragment shader drawing accent-tinted, bottom-anchored bars from `audio_stages`' smoothed spectrum textures.
- `visualizer_shaders.h`+`.cpp`: Concatenates the `assets/shaders/visualizer/sphere/*.glsl` fragments into the flattened sphere/glow shaders at runtime, since astralia-shell has no shader preprocessor.

## src/modules/lock

- `layout.h`+`.cpp`: Pure lock-panel geometry math: card size, three-column split, content-stack height, and dot row; test-linked, no `EGL`.
- `pam_authenticator.h`+`.cpp`: `pam_start_confdir`-based password check against the shipped `astralia-shell` `PAM` service, with a `login` fallback; runs off the poll thread.

## src/modules/rain

- `matrix_rain.h`+`.cpp`: `MatrixRain`: per-column falling glyph sim with an accent decay trail, Cairo-rasterized each step; `async_speed` randomizes each column's fall rate.
- `stiletto_rain.h`+`.cpp`: `StilettoRain`: a falling-comet sim with a single connected accent tail and a composited `stiletto.svg` head; `async_speed` randomizes fall rate.

## src/modules/launcher

- `apps_provider.h`+`.cpp`: App name scoring and search over `DesktopEntry` lists.
- `desktop_entry.h`+`.cpp`: `.desktop` file parsing, scanning, and launch dispatch.
- `files_provider.h`+`.cpp`: Path scoring and `fd`-backed file/dir search.
- `launch_action.h`+`.cpp`: URL/run/web-search launch dispatch.
- `search.h`+`.cpp`: Query-mode prefix detection and combined result ranking.
- `submenu.h`+`.cpp`: Directory-browse submenu navigation and entry actions.
- `visit_store.h`+`.cpp`: Per-item launch-count persistence for result ranking.

## src/modules/settings

- `wallpaper_tab.h`+`.cpp`: Per-tab settings UI and commit logic.
- `displays_tab.h`+`.cpp`: Per-tab settings UI and commit logic.
- `idle_tab.h`+`.cpp`: Per-tab settings UI and commit logic.
- `logout_tab.h`+`.cpp`: Per-tab settings UI and commit logic (central-logo static/animated toggle).
- `visualizer_tab.h`+`.cpp`: Per-tab settings UI and commit logic; `Bar`/`Sphere` shape selector, then a per-`VisualizerParams`-knob number field row.
- `rain_tab.h`+`.cpp`: Per-tab settings UI and commit logic; `Matrix`/`Stiletto` `RainMode` selector row plus an `Asynchronous fall speed` toggle row.
- `animation_tab.h`+`.cpp`: Per-tab settings UI and commit logic; single `Disable Animations` toggle row.

## src/modules/bar

- `panel/network_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/bluetooth_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/volume_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/tray_panel.h`+`.cpp`: On-demand tray grid panel plus its context menu, a separate `xdg_popup` grabbed to the panel layer surface.
- `panel/battery_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/system_monitor_panel.h`+`.cpp`: One `<name>_panel.h/.cpp` pair per on-demand panel.
- `panel/clock_panel.h`+`.cpp`: On-demand centered month-grid calendar panel; header prev/today/next month nav, weekday row, `6x7` day grid with today highlighted.
- `widget/widget_capsule.h`+`.cpp`: Shared pill bookkeeping, hover-expand/click dispatch, and the pill-row layout/draw.
- `widget/workspace_widget.h`+`.cpp`: Workspace-row drawing plus trailing overview-toggle icon; records per-pill and icon hit rects for click routing.
- `widget/dock_widget.h`+`.cpp`: Bar-capsule variant of the dock icon row for the active workspace, drawn after the workspace row via shared `render/dock_row`; non-interactive.
- `widget/clock_widget.h`+`.cpp`: State-free clock-pill drawing; returns the pill hit rect and owns the calendar-panel open trigger.
- `widget/logout_widget.h`+`.cpp`: One pair per bar pill.
- `widget/battery_widget.h`+`.cpp`: One pair per bar pill.
- `widget/network_widget.h`+`.cpp`: One pair per bar pill.
- `widget/bluetooth_widget.h`+`.cpp`: One pair per bar pill.
- `widget/volume_widget.h`+`.cpp`: One pair per bar pill.
- `widget/dashboard_widget.h`+`.cpp`: One pair per bar pill.
- `widget/system_monitor_widget.h`+`.cpp`: One pair per bar pill (CPU pill opening the system-monitor panel).
- `widget/tray_widget.h`+`.cpp`: One pair per bar pill (tray pill opening the tray panel).

## src

- `astralia-shell.cpp`: Orchestration, EGL bootstrap, poll loop, CLI entry point, daemonize/debug/`start-lock`/IPC-client dispatch. `backend_connect()` probes Wayland then X11; `backend_bootstrap()`/`backend_poll_*()` (`app/backend_bootstrap.h`+`backend_poll.h`) do the actual backend-specific registry/`XRandR` handshake and per-iteration flush/fd/dispatch, so this file itself has no backend branch.

## src/wayland

Wayland bodies (namespace `backend_wayland`) behind the seams declared in `src/render/`+`src/service/`; each mirrors the pre-split implementation with no behavior change.

- `egl_surface.h`+`.cpp`: `wl_egl_window`/`eglCreateWindowSurface`-based `NativeEglWindowHandle` creation, plus `wl_surface_commit`/`wl_surface_set_input_region`.
- `layer_surface.h`+`.cpp`: `zwlr_layer_surface_v1`-backed `LayerSurfaceHandle`; owns a small heap-allocated wrapper pairing the raw Wayland object with the plain `LayerSurfaceConfigureFn` adapter, since `zwlr_layer_surface_v1_add_listener` needs a concrete Wayland-typed listener.
- `toplevel_window.h`+`.cpp`, `popup_window.h`+`.cpp`: `xdg_toplevel`/`xdg_popup` bodies, unchanged from before the backend split.
- `frame_service.h`+`.cpp`: `wl_surface_frame`/`wl_callback`-paced redraw, unchanged from before the split.
- `input_service.h`+`.cpp`: `wl_seat`/`wl_keyboard`/`wl_pointer`/`wp_cursor_shape_manager_v1` listeners, unchanged from before the split.
- `capture_service.h`+`.cpp`: `hyprland-toplevel-export-v1` capture, unchanged from before the split.
- `output_service.h`+`.cpp`: The real `wl_surface.preferred_buffer_scale` listener body (moved out of `service/output_service.cpp`).
- `bootstrap.h`+`.cpp`: `wl_registry` bind/listener wiring (`registry_global`/`registry_global_remove`, output/`xdg_wm_base` listeners) - the real body behind `app/backend_bootstrap.h`'s Wayland branch; moved here from `app/wayland_registry.cpp`.
- `session_lock.h`+`.cpp`: The real `ext_session_lock_v1`/`ext_session_lock_surface_v1` listener bodies and surface lifecycle, behind `modules/lock.cpp`'s `session_lock_*` dispatch.
- `text_input_service.cpp`: The real `zwp_text_input_v3` `TextInputService` method bodies, moved as-is from `service/text_input_service.cpp`.

## src/x11

X11 bodies (namespace `backend_x11`) behind the same seams. `astralia-shell` renders and takes input under `awesome`+`picom` through this backend as of the X11 support work (`local/plan/x11-full-support.md`); live output hotplug, a real `i3lock`-style session-lock grab, and a real `XIM` IME client remain out of scope.

- `egl_surface.h`+`.cpp`: An X `Window` XID is itself a valid `EGLNativeWindowType`, so `egl_native_window_create` is a no-op; `native_surface_set_input_region` is a documented no-op pending `XShapeCombineRegion`.
- `atoms.h`+`.cpp`: `XInternAtom`-cached EWMH atoms (`_NET_WM_STRUT`, `_NET_WM_STRUT_PARTIAL`, `_NET_WM_DESKTOP`, `_NET_WM_WINDOW_TYPE`, `_NET_WM_WINDOW_TYPE_DOCK`, `_NET_WM_STATE`, `_NET_WM_STATE_ABOVE`, `_NET_WM_STATE_BELOW`, `_NET_WM_NAME`, `UTF8_STRING`, `WM_PROTOCOLS`, `WM_DELETE_WINDOW`).
- `output_bootstrap.h`+`.cpp`: `XRandR`-based per-monitor output enumeration (`bootstrap_outputs`), a synthetic `wl_output*` identity token per connected output with an active `CRTC`, and `monitor_rect` mapping that token back to its `CRTC` rect for per-output geometry.
- `layer_surface.h`+`.cpp`: `_NET_WM_WINDOW_TYPE_DOCK` `Window` with a 32-bit ARGB visual, WM-managed (not override-redirect) so `_NET_WM_STATE_ABOVE`/`_NET_WM_STATE_BELOW` (from `cfg.layer`) is honored for stacking; EWMH struts computed from anchor/margin/exclusive-zone against the owning monitor's `XRandR` rect (offset into the root screen's edges per EWMH's whole-screen strut convention), via `output_bootstrap.h`'s `monitor_rect`. `WM_NORMAL_HINTS`' `PPosition`/`PSize` flags mark the requested geometry as deliberate; a `Window`-to-owner table plus `layer_surface_handle_configure_notify` (routed from `app/backend_poll.cpp` alongside `toplevel_window`'s handler) re-applies that geometry if the window manager moves or resizes the surface. `layer_surface_set_keyboard_interactivity` drives `XSetInputFocus` directly.
- `toplevel_window.h`+`.cpp`: A real (non-override-redirect) WM-managed `Window` with `WM_NORMAL_HINTS`/`_NET_WM_NAME`/`WM_CLASS`/`WM_DELETE_WINDOW` set; a `Window`-to-owner table plus `toplevel_window_handle_configure_notify`/`_handle_client_message` let `app/backend_poll.cpp` route live resize and close-button events back to it.
- `popup_window.h`+`.cpp`: Override-redirect `Window` positioned at the anchor rect's bottom-left corner (clamped to screen), approximating the Wayland positioner's anchor/gravity/slide behavior; no live grab yet.
- `frame_service.h`+`.cpp`: No per-surface frame-callback protocol exists on core X11 (`picom` owns presentation timing); `request_frame` redraws immediately rather than pacing through a callback.
- `input_service.h`+`.cpp`: `XInput2`/`libxkbcommon-x11` (bridged to XCB via `XGetXCBConnection`) keymap/event setup, `handle_xi_device_event` (the `XIDeviceEvent`-to-`KeyboardState`/`PointerState` translator), and `xi_opcode()` caching the `XInputExtension` opcode for `app/backend_poll.cpp`'s `GenericEvent` routing.
- `capture_service.h`+`.cpp`: `XCompositeNameWindowPixmap`+`XGetImage` capture, reading back the common 32-bit TrueColor BGRA layout; genuinely more capable than the Wayland path since it needs no `i3`-side protocol support (see `local/plan/x11-support.md`'s "Live-thumbnail capture gets more portable, not less").
- `output_service.h`+`.cpp`: `output_scale_watch` no-op stub - core X11 has no live per-surface preferred-scale event.
- `bootstrap.h`+`.cpp`: The real body behind `app/backend_bootstrap.h`'s X11 branch - calls `output_bootstrap.h`'s `bootstrap_outputs`, then `keyboard_attach_seat`/`pointer_bind` directly (no `wl_seat` registry equivalent to wait for).
- `session_lock.h`+`.cpp`: Stub - `session_lock_acquire` always fails; a real `i3lock`-style grab is unimplemented.
- `text_input_service.cpp`: All-methods-no-op `TextInputService` stub; compiled in place of `wayland/text_input_service.cpp` only when `wayland_backend` is disabled (real `XIM` support is unimplemented).

## test

`test/`: One test file per pure-logic header, grouped by module, run through one `astralia-shell-test` binary via meson.

- astralia-shell-test.cpp
- astralia-shell-test.hpp
- app/test_config.cpp
- app/test_wallpaper_resolve.cpp
- core/test_async_process.cpp
- core/test_deferred_call.cpp
- core/test_path_home.cpp
- core/test_poll_source.cpp
- dbus/test_network_parse.cpp
- dbus/test_bluetooth.cpp
- launcher/test_launcher.cpp
- wayland/test_keyboard.cpp
- wayland/test_active_output.cpp
- wayland/test_dock.cpp
- system/test_rfkill.cpp
- render/test_animation.cpp
- render/test_animated_image.cpp
- render/test_marquee_scroll.cpp
- render/test_palette.cpp
- render/test_image_decode.cpp
- render/test_text_elide.cpp
- lock/test_layout.cpp
- visualizer/test_fft.cpp
- dbus/test_mpris.cpp
- system/test_cpu_temp.cpp
- system/test_gpu_temp.cpp
- system/test_system_stats.cpp

## root

- `meson.build`: Build config, dependency list, test registration; `wayland_backend`/`x11_backend` (`meson_options.txt`) independently gate their dev-package `dependency()` calls and `src/wayland/`+`src/x11/` source lists, with `ASTRALIA_HAVE_WAYLAND`/`ASTRALIA_HAVE_X11` compile defines guarding every dispatch shim's matching branch; configure fails if both resolve disabled. The test binary requires `wayland_backend` (its EGL context creation is Wayland-only) and isn't built when it's disabled.
- `meson_options.txt`: The `wayland_backend`/`x11_backend` feature options (`auto` by default).
- `convention.md`: Formatting and commenting rules.

## dist

- `build.sh`: Shared configure+compile step (RAM-capped job count via `ASTRALIA_SHELL_BUILD_JOBS`), called by `test.sh` and `install.sh`.
- `{run,install,test}`: Convenience scripts to build+test, build+install, or kill+install+launch astralia-shell.

## assets

- `fonts/*`, `constellation/C*.png`: Installed fonts, launcher constellation bullet icons.
- `shaders/**`: Every `#version 100` GLES shader the shell compiles, grouped by consumer directory; installed as a subdir by meson.
- `NOTICE`: Third-party attribution for ported shader and asset parts.
- `stellar-restoration.png`: Default wallpaper wallpaper, the `ASTRALIA_SHELL_DEFAULT_WALLPAPER` fallback when a column has no configured path.
- `stellar-restoration.svg`: Idle screensaver bouncing-logo source (placeholder).
- `stiletto.svg`: `stiletto_rain` comet head, rasterized once aspect-correct and scaled to the comet-row head height.
- `electro.png`: Password-field echo glyph, drawn per character.
- `gifs/profile.gif`: Lock avatar, settings and dashboard profile-picture source, decoded to cached frames via `ffmpeg`.
- `logout/logo.gif`: Logout animated centre-logo source, decoded to cached frames via `ffmpeg`.
- `logout/logo.png`: Logout static centre-logo source, used when the animated-logo toggle is off.
- `pam/astralia-shell`: `PAM` service file for the lock screen, loaded via `pam_start_confdir`.

## protocols

- `wlr-layer-shell-unstable-v1.xml`: Wayland protocol XML, code-generated at build time.
- `hyprland-toplevel-export-v1.xml`: Per-window live-capture protocol XML, code-generated, used by `overview`.
- `text-input-unstable-v3.xml`: IME text-input protocol XML, code-generated, used by `text_input_service`.
- `wlr-foreign-toplevel-management-unstable-v1.xml`: Unused directly; linked only to satisfy a symbol `hyprland-toplevel-export-v1`'s v2 request references.
- `ext-session-lock-v1`: From `wayland-protocols` (`staging/`), code-generated at build time, used by `lock`.

## local

- `local/`: Planning/design docs, not part of the shipped repo.
