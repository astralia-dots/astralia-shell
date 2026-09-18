#include <GLES2/gl2.h>

#include "app/monitor_output.h"
#include "app/wayland_registry.h"
#include "app/wayland_state.h"

#include "core/log.h"

#include "modules/bar.h"

#include "render/egl_surface.h"

void bar_layer_surface_configure(void *data, int32_t width, int32_t) {
    auto *mon = static_cast<MonitorOutput *>(data);
    mon->width = width;
    if (mon->egl_window) {
        int32_t scale = mon->output_scale.scale;
        egl_native_window_resize(mon->egl_window, mon->width * scale, bar_detail::bar_current_height(*mon) * scale);
    }
    mon->configured = true;
}

bool bootstrap_egl(WaylandState &state) {
    state.egl_display =
        eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(state.display));
    if (state.egl_display == EGL_NO_DISPLAY)
        return false;
    if (!eglInitialize(state.egl_display, nullptr, nullptr))
        return false;
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE,
    };
    EGLint num_configs = 0;
    if (!eglChooseConfig(state.egl_display, config_attribs, &state.egl_config, 1, &num_configs) || num_configs == 0) {
        return false;
    }

    const EGLint context_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    state.egl_context = eglCreateContext(state.egl_display, state.egl_config, EGL_NO_CONTEXT, context_attribs);
    if (state.egl_context == EGL_NO_CONTEXT) {
        klog("egl: OpenGL ES 2.0 context creation failed, egl error 0x%04x", eglGetError());
        return false;
    }
    return true;
}

bool renderer_bootstrap_init(WaylandState &state) {
    const EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface pbuffer = eglCreatePbufferSurface(state.egl_display, state.egl_config, pbuffer_attribs);
    if (pbuffer == EGL_NO_SURFACE)
        return false;
    if (!eglMakeCurrent(state.egl_display, pbuffer, pbuffer, state.egl_context)) {
        eglDestroySurface(state.egl_display, pbuffer);
        return false;
    }
    klog("gl: %s | GLSL %s", reinterpret_cast<const char *>(glGetString(GL_VERSION)), reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    bool ok = state.renderer.init();
    eglDestroySurface(state.egl_display, pbuffer);
    return ok;
}
