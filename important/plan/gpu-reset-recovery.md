# GPU reset recovery

## Goal

When the GPU loses its context, for example on an NVIDIA resume with a video-memory purge or a driver reset, the shell should notice it, rebuild the context, and keep drawing. Without recovery it goes black or freezes silently. The approach follows noctalia's `Application::recoverGraphicsAfterReset`, adapted to astralia's single shared context.

## Status (2026-09-24)

- The structure refactor and its runtime checks passed.
- 2a (detect and log) is done, committed in `ba5d516`, and `./build.sh test` passes.
- On the NVIDIA machine (RTX 4070 Ti SUPER), startup logs `egl: robust context, lose on reset, video memory purge` and `gl: reset detection via glGetGraphicsResetStatus`.
- 2b and 2c are not started. Whether they are needed depends on the checks below.

## Next: checks on the real machine

1. Open and close the visualizer once. A `visualizer: eglCreateContext failed 0x3009` (`EGL_BAD_MATCH`) line means the shared-context attributes don't match.
2. Run `grep PreserveVideoMemoryAllocations /proc/driver/nvidia/params`. A value of `1` makes a purge reset unlikely.
3. Watch `tail -F ~/.local/state/astralia/astralia.log | grep --line-buffered -E 'gl: graphics reset|eglMakeCurrent failed'` across several suspend and resume cycles.

Decision: if a `gl: graphics reset detected` line is followed by a black or frozen shell, do 2b and then 2c. If no reset shows up after several suspends, stop at 2a and close this plan.

## How 2a works

- `bootstrap_egl` (`src/app/wayland_registry.cpp`) asks for `EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT` = `EGL_LOSE_CONTEXT_ON_RESET_EXT`. It adds `EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV` when `EGL_NV_robustness_video_memory_purge` exists, and falls back to a plain context if either is missing or creation fails.
- `WaylandState::egl_context_attribs` keeps the attributes the context was created with. `visualizer` builds its shared context from them, because EGL rejects a shared context whose reset strategy differs (`EGL_BAD_MATCH`).
- `gl_reset_detection_init` (`render/gl`) resolves `glGetGraphicsResetStatus`, or the `KHR`/`EXT` variant, after the first make-current.
- `gl_poll_graphics_reset` runs on the 1 Hz timer in `main.cpp` and after a failed `gl_make_current`. It logs only when the status changes, and keeps the last status per thread, because the visualizer thread also reaches it through `gl_make_current`.

## Facts the design relies on

- Astralia has one `EGLContext` (`WaylandState::egl_context`). `visualizer` creates a second one, shared with it, for its render thread.
- `EGLSurface`s belong to the display and config, not the context. They survive a context swap, as noctalia also relies on, so no surface needs recreating.
- Only 6 of the 42 `eglSwapBuffers` calls check their result. A failed `gl_make_current` is logged and the frame skipped, but nothing yet tells a reset apart from a transient failure.

## GL objects to invalidate

| Owner | Objects | How they come back |
|---|---|---|
| `Renderer` (`render/renderer`) | 5 programs, `quad_vbo_` | Call `Renderer::init()` again. |
| `Texture` (`render/texture`), everywhere | RAII texture handles in about 30 headers (bar 13, lock 4, logout 5, launcher 3, osd 2, notification 3, polkit 2, settings 2, panels, widgets) | Most are caches or rasterizations (see below). |
| `TextureCache`, `DockIconCache`, `art_cache` maps | Keyed rasterized text, icons, images | Clear the map. The next paint rebuilds lazily. |
| Bar fillet and hug textures | Built when their pixel size changes | Clear, and zero `fillet_px`/`hug_px` so the size check rebuilds them. |
| Bar and other stub icon textures (`init_stub_widgets`, clock) | Built at init and on the clock tick | Call the builder again. |
| `notification.cpp:70` | A function-local `static Texture` close icon | Must become a resettable member or cache entry. A `static` can't be rebuilt. |
| `AnimatedImage` (`render/animated_image`) | Frame textures from the `media_service` `.rgba` cache | `hide` then `show`, which re-uploads from the CPU cache. |
| `wallpaper` | Per-column textures, `VideoTexture` `EGLImage`s, one transition program | Drop the GL side, keep the decode, and re-upload the pending frame. `VideoTexture` needs its `EGLImage` destroyed and re-imported from the next `DrmFrameImport`. |
| `logout` | 2 lazily compiled shader programs, 1 texture | Zero the program ids so the lazy compile runs again. |
| `rain` sims | 1 texture each | Mark dirty. They re-rasterize every step anyway. |
| `settings/wallpaper_tab` | Thumbnail textures | Clear, and they re-upload from the decoded thumbnails. |
| `overview`, `capture_service` | Capture textures | Clear. The next capture re-uploads. |
| `visualizer` (`audio_stages`, `sphere_visualizer`, `bar_visualizer`) | 6 programs, 5 textures, 3 FBOs, 4 VBOs on the render thread's shared context | Stop the render thread and destroy the shared context. Restart on the next open, or immediately if it's open. |

## 2b: Abandon-not-delete plumbing

The hazard: `Texture::reset()` calls `glDeleteTextures` on its stored id. After a reset that id is meaningless, but it can equal an id the new context has already handed to a different, live texture. Destroying old objects after the swap would then delete valid new textures, which shows up as random missing icons.

- Add `uint32_t gl_generation` in `render/gl`, read through `gl_context_generation()`.
- `Texture` records the generation it was created in. `reset()` calls `glDeleteTextures` only when that matches the current generation. Otherwise it just forgets the id.
- Apply the same guard wherever raw `GLuint`s are deleted: `Renderer`, `VideoTexture`, `logout`, and `wallpaper`. Visualizer objects live on its own thread and context, so that context gets torn down whole instead.
- Make the `static Texture` close icon at `src/modules/notification.cpp:70` resettable.
- This is pure plumbing, with no behavior change while no reset happens.

## 2c: Recovery

- Add `Module::on_gl_reset(WaylandState &)` and `PerMonitorModule::on_gl_reset(WaylandState &, MonitorOutput &)`, both no-ops by default.
- Implement them, following the table above, in:
  - `bar` (with its panels and widgets)
  - `launcher`, `lock`, `logout`, `notification`, `osd`, `overview`, `polkit`
  - `rain`, `settings`, `wallpaper`, `idle`, `visualizer`
- When `gl_poll_graphics_reset` reports a reset other than `GL_NO_ERROR`, schedule recovery once with `DeferredCall::call_later`. Never run it inside a paint callback. The sequence:
  1. Bump `gl_generation`.
  2. Run every `on_gl_reset` hook.
  3. The visualizer stops its render thread and destroys its shared context.
  4. Call `eglMakeCurrent(EGL_NO_CONTEXT)`, destroy the old context, create a new one from `egl_context_attribs`, and rest on it.
  5. Run `Renderer::init()` again.
  6. Request a frame on every surface: every module's `request_frame`, and the lock's `request_all`.
- If creating the new context fails, retry with backoff from 250 ms, doubling up to 4 s, for at most 5 attempts, and log each attempt. After the last attempt, log an error and stop drawing rather than looping.
- Add an IPC verb `gl-reset-test` that runs steps 1 to 6 on demand without a real reset. It exercises every hook and the abandon guard, because the old context is really destroyed. Afterwards, check that these look the same as before:
  - the bar and any open panels
  - the wallpaper, static and animated
  - the lock screen, if locked
  - the visualizer, if open
  - the logout shaders
- On NVIDIA, suspend and resume is the real-world check. `NVreg_PreserveVideoMemoryAllocations=0` usually triggers a purge reset.

## After each step

- Run `./build.sh test`, then `clang-format -i` on the touched files, and stop for review.
- `important/index.md`: update the entry of every touched file.
- `important/critical-knowledge.md`: add anything the step teaches.
- `important/convention.md`: re-check compliance, including no comments and root-relative includes in header order.
