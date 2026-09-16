#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "config/visualizer_config.h"

#include "core/log.h"

#include "modules/visualizer/sphere_visualizer.h"
#include "modules/visualizer/visualizer_shaders.h"

#include "render/gl.h"
#include "render/palette.h"

#include <GLES2/gl2ext.h>

namespace {

constexpr GLfloat kQuadVerts[18] = {-1, -1, 0, 1, -1, 0, -1, 1, 0,
                                    1,  1,  0, 1, -1, 0, -1, 1, 0};

int visualizer_canvas_size(int width, int height) {
    int smaller = width < height ? width : height;
    int size = static_cast<int>(static_cast<float>(smaller) * kVisualizerCanvasFraction);
    return size < kVisualizerCanvasMin ? kVisualizerCanvasMin : size;
}

// Mirrors sphere1_vert_main.glsl's (and the deleted sphere1_main.glsl's)
// per-pixel dropout hash exactly:
// fract(sin(mod(dot(floor(gl_FragCoord.xy), vec2(127.1, 311.7)), TWOPI)) * 43758.5453123)
float particle_thin_hash(float px, float py) {
    constexpr float kTwoPi = 6.2831853071794f;
    float d = px * 127.1f + py * 311.7f;
    float m = std::fmod(d, kTwoPi); // dot() is always >= 0 here, matching GLSL mod()
    float s = std::sin(m) * 43758.5453123f;
    return s - std::floor(s);
}

} // namespace

bool SphereVisualizer::init() {
    if (ready_)
        return true;

    std::string fullscreen_vs = gl_load_shader("visualizer/fullscreen.vert");
    std::string sphere1_vs = visualizer_shaders::sphere1_vs();
    std::string sphere2 = visualizer_shaders::sphere2_fs();
    std::string glow = visualizer_shaders::glow_fs();
    std::string sphere1_splat_fs = gl_load_shader("visualizer/sphere/sphere1_splat.frag");

    sphere1_prog_ = gl_compile_program(sphere1_vs.c_str(), sphere1_splat_fs.c_str(), "visualizer_sphere1");
    sphere2_prog_ = gl_compile_program(fullscreen_vs.c_str(), sphere2.c_str(), "visualizer_sphere2");
    glow_prog_ = gl_compile_program(fullscreen_vs.c_str(), glow.c_str(), "visualizer_glow");
    present_prog_ = gl_compile_program_files("visualizer/sphere/present.vert", "visualizer/sphere/present.frag", "visualizer_present");
    if (!sphere1_prog_ || !sphere2_prog_ || !glow_prog_ || !present_prog_) {
        klog("visualizer_sphere: shader compile failed");
        return false;
    }

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &particle_vbo_);

    ready_ = true;
    return true;
}

void SphereVisualizer::destroy() {
    GLuint progs[] = {sphere1_prog_, sphere2_prog_, glow_prog_, present_prog_};
    for (GLuint p : progs)
        if (p)
            glDeleteProgram(p);
    for (GLuint t : fbo_tex_)
        if (t)
            glDeleteTextures(1, &t);
    for (GLuint f : fbo_)
        if (f)
            glDeleteFramebuffers(1, &f);
    if (glow_tex_)
        glDeleteTextures(1, &glow_tex_);
    if (glow_fbo_)
        glDeleteFramebuffers(1, &glow_fbo_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (particle_vbo_)
        glDeleteBuffers(1, &particle_vbo_);
    *this = SphereVisualizer{};
}

void SphereVisualizer::ensure_targets(int canvas) {
    if (fbo_tex_[0] && canvas == canvas_)
        return;

    for (GLuint &t : fbo_tex_)
        if (t) {
            glDeleteTextures(1, &t);
            t = 0;
        }
    for (GLuint &f : fbo_)
        if (f) {
            glDeleteFramebuffers(1, &f);
            f = 0;
        }
    if (glow_tex_) {
        glDeleteTextures(1, &glow_tex_);
        glow_tex_ = 0;
    }
    if (glow_fbo_) {
        glDeleteFramebuffers(1, &glow_fbo_);
        glow_fbo_ = 0;
    }

    canvas_ = canvas;

    // fbo_[0]/fbo_tex_[0] is the point-sprite additive-blend target (replaces
    // the old atomic_tex_ accumulator); sphere2_prog_ samples it as `tex`.
    // It needs GL_FLOAT, not GL_UNSIGNED_BYTE: the shell-evacuation math in
    // sphere1_vert_main.glsl can pile hundreds of overlapping splats onto the
    // same pixel (the whole inner ~55% of the particle disc converges onto a
    // thin ring), so the accumulated value routinely exceeds 1.0. An 8-bit
    // UNORM target hard-clamps there, starving sphere2_main.glsl's
    // actualDepth-driven brightness curve (ported verbatim from the ES3.2
    // atomic accumulator, which had no such ceiling). GL_OES_texture_float +
    // GL_EXT_color_buffer_float (render) + GL_EXT_float_blend (additive
    // blending into it) are all present on this hardware.
    GLuint *tex[] = {&fbo_tex_[0], &fbo_tex_[1], &glow_tex_};
    GLuint *fbo[] = {&fbo_[0], &fbo_[1], &glow_fbo_};
    for (int i = 0; i < 3; ++i) {
        // GL_EXT_color_buffer_float on ES 2.0 requires the sized internal
        // format token passed directly to glTexImage2D to make the texture
        // framebuffer-attachable; the unsized GL_RGBA/GL_FLOAT combo compiles
        // and samples fine but every draw into it fails framebuffer
        // completeness (GL_INVALID_FRAMEBUFFER_OPERATION).
        GLenum internal_format = (i == 0) ? GL_RGBA32F_EXT : GL_RGBA;
        GLenum type = (i == 0) ? GL_FLOAT : GL_UNSIGNED_BYTE;
        glGenTextures(1, tex[i]);
        glBindTexture(GL_TEXTURE_2D, *tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, canvas, canvas, 0, GL_RGBA, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SphereVisualizer::ensure_particle_grid(int canvas, float particle_thin) {
    if (particle_vbo_ && canvas == particle_grid_canvas_ && particle_thin == particle_grid_thin_)
        return;

    particle_grid_canvas_ = canvas;
    particle_grid_thin_ = particle_thin;

    std::vector<float> points;
    points.reserve(static_cast<size_t>(canvas) * static_cast<size_t>(canvas) * 2);
    for (int py = 0; py < canvas; ++py) {
        for (int px = 0; px < canvas; ++px) {
            if (particle_thin_hash(static_cast<float>(px), static_cast<float>(py)) < particle_thin)
                continue;
            points.push_back(static_cast<float>(px) + 0.5f);
            points.push_back(static_cast<float>(py) + 0.5f);
        }
    }
    particle_count_ = static_cast<int>(points.size() / 2);

    glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(float)), points.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SphereVisualizer::draw_quad() {
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
}

void SphereVisualizer::set_audio_uniforms(GLuint prog, GLuint audio_l_tex, GLuint audio_r_tex, int audio_size, int tick, int canvas, const VisualizerParams &params) {
    glUniform2f(glGetUniformLocation(prog, "resolution"), static_cast<float>(canvas), static_cast<float>(canvas));
    glUniform1f(glGetUniformLocation(prog, "time"), static_cast<float>(tick));
    glUniform1i(glGetUniformLocation(prog, "audioRSize"), audio_size);
    glUniform1i(glGetUniformLocation(prog, "audioLSize"), audio_size);
    glUniform3f(glGetUniformLocation(prog, "u_accent"), palette::accent.r, palette::accent.g, palette::accent.b);
    glUniform1i(glGetUniformLocation(prog, "u_particleSize"), params.particle_size);
    glUniform1i(glGetUniformLocation(prog, "u_complexity"), params.fractal_complexity);
    glUniform1f(glGetUniformLocation(prog, "u_glowDirections"), params.glow_directions);
    glUniform1f(glGetUniformLocation(prog, "u_glowQuality"), params.glow_quality);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, audio_r_tex);
    glUniform1i(glGetUniformLocation(prog, "audioR"), 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, audio_l_tex);
    glUniform1i(glGetUniformLocation(prog, "audioL"), 2);

    glActiveTexture(GL_TEXTURE0);
}

void SphereVisualizer::render(int width, int height, int tick, float fade, GLuint audio_l_tex, GLuint audio_r_tex, int audio_size, const VisualizerParams &params) {
    if (!ready_ || width <= 0 || height <= 0)
        return;

    int canvas = visualizer_canvas_size(width, height);

    static int trace_frames = 8;
    bool trace = trace_frames > 0;
    if (trace)
        --trace_frames;
    auto mark = [trace, tick](const char *tag) {
        if (!trace)
            return;
        auto t0 = std::chrono::steady_clock::now();
        glFinish();
        klog("visualizer_sphere: f%d %s %.1fms", tick, tag, std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count());
    };
    // TEMP DIAGNOSTIC: stats (min/avg/max/stddev of the red channel) of the raw
    // float accumulator, to find whether local variance survives into it or is
    // already gone by the time sphere2 reads it.
    auto dump_stats_r_float = [trace, tick, canvas](GLuint fbo, const char *tag) {
        if (!trace)
            return;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        std::vector<float> buf(static_cast<size_t>(canvas) * static_cast<size_t>(canvas) * 4);
        glReadPixels(0, 0, canvas, canvas, GL_RGBA, GL_FLOAT, buf.data());
        double sum = 0, sumsq = 0;
        float mn = 1e30f, mx = -1e30f;
        size_t n = buf.size() / 4;
        for (size_t i = 0; i < buf.size(); i += 4) {
            float r = buf[i];
            sum += r;
            sumsq += static_cast<double>(r) * r;
            mn = std::min(mn, r);
            mx = std::max(mx, r);
        }
        double mean = sum / n;
        double stddev = std::sqrt(std::max(0.0, sumsq / n - mean * mean));
        klog("visualizer_sphere: f%d %s(float) min=%.4f mean=%.4f max=%.4f stddev=%.4f", tick, tag, mn, mean, mx, stddev);
    };
    // Same stats, for an 8-bit UNORM target's red channel (0..255 space).
    auto dump_stats_r_u8 = [trace, tick, canvas](GLuint fbo, const char *tag) {
        if (!trace)
            return;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        std::vector<unsigned char> buf(static_cast<size_t>(canvas) * static_cast<size_t>(canvas) * 4);
        glReadPixels(0, 0, canvas, canvas, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        double sum = 0, sumsq = 0;
        int mn = 255, mx = 0;
        size_t n = buf.size() / 4;
        for (size_t i = 0; i < buf.size(); i += 4) {
            int r = buf[i];
            sum += r;
            sumsq += static_cast<double>(r) * r;
            mn = std::min(mn, r);
            mx = std::max(mx, r);
        }
        double mean = sum / n;
        double stddev = std::sqrt(std::max(0.0, sumsq / n - mean * mean));
        klog("visualizer_sphere: f%d %s(u8) min=%d mean=%.2f max=%d stddev=%.2f", tick, tag, mn, mean, mx, stddev);
    };

    bool first_targets = fbo_tex_[0] == 0 || canvas != canvas_;
    ensure_targets(canvas);
    if (first_targets)
        mark("ensure_targets");

    ensure_particle_grid(canvas, params.particle_thin);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    for (GLuint fbo : {fbo_[0], fbo_[1], glow_fbo_}) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, canvas, canvas);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // ES 2.0 has no image load/store: fbo_[0] accumulates particle splats via
    // additive blending instead of imageAtomicAdd (see sphere1_vert_head.glsl).
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[0]);
    glUseProgram(sphere1_prog_);
    set_audio_uniforms(sphere1_prog_, audio_l_tex, audio_r_tex, audio_size, tick, canvas, params);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, particle_count_);
    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
    mark("sphere1");
    dump_stats_r_float(fbo_[0], "sphere1_accum");

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[1]);
    glUseProgram(sphere2_prog_);
    set_audio_uniforms(sphere2_prog_, audio_l_tex, audio_r_tex, audio_size, tick, canvas, params);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex_[0]);
    glUniform1i(glGetUniformLocation(sphere2_prog_, "tex"), 0);
    draw_quad();
    mark("sphere2");
    dump_stats_r_u8(fbo_[1], "sphere2_out");

    glBindFramebuffer(GL_FRAMEBUFFER, glow_fbo_);
    glUseProgram(glow_prog_);
    set_audio_uniforms(glow_prog_, audio_l_tex, audio_r_tex, audio_size, tick, canvas, params);
    glUniform1f(glGetUniformLocation(glow_prog_, "u_fade"), fade);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex_[1]);
    glUniform1i(glGetUniformLocation(glow_prog_, "tex"), 0);
    draw_quad();
    mark("glow");

    present(width, height, canvas, fade);
    mark("present");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void SphereVisualizer::present(int width, int height, int canvas, float fade) {
    int off_x = (width - canvas) / 2;
    int off_y = (height - canvas) / 2;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(palette::window_backdrop.r, palette::window_backdrop.g, palette::window_backdrop.b, palette::window_backdrop.a * fade);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(off_x, off_y, canvas, canvas);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(present_prog_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glow_tex_);
    glUniform1i(glGetUniformLocation(present_prog_, "tex"), 0);
    draw_quad();
    glDisable(GL_BLEND);
}
