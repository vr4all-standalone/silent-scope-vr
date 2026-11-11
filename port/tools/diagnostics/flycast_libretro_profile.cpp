// Private native throughput diagnostic; never an XR or physical-play measurement.
// Build against the pinned Flycast core/deps/libretro-common/include/libretro.h.
#include "flycast_profile_policy.h"
#include <libretro.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <limits.h>
#include <stdexcept>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) { return std::chrono::duration<double>(Clock::now() - start).count(); }
volatile std::sig_atomic_t stop_requested = 0;
void on_signal(int) { stop_requested = 1; }
constexpr unsigned surface_size = 1024;

struct Api {
    decltype(&retro_api_version) version = nullptr;
    decltype(&retro_set_environment) set_environment = nullptr;
    decltype(&retro_set_video_refresh) set_video = nullptr;
    decltype(&retro_set_audio_sample) set_audio = nullptr;
    decltype(&retro_set_audio_sample_batch) set_audio_batch = nullptr;
    decltype(&retro_set_input_poll) set_poll = nullptr;
    decltype(&retro_set_input_state) set_input = nullptr;
    decltype(&retro_init) init = nullptr;
    decltype(&retro_deinit) deinit = nullptr;
    decltype(&retro_get_system_info) system_info = nullptr;
    decltype(&retro_get_system_av_info) av_info = nullptr;
    decltype(&retro_set_controller_port_device) controller = nullptr;
    decltype(&retro_load_game) load = nullptr;
    decltype(&retro_unload_game) unload = nullptr;
    decltype(&retro_run) run = nullptr;
};
struct SnapshotInfo { unsigned width, height; double native_audio_seconds; };

struct State {
    std::string root, system_dir, save_dir, library, cue;
    std::map<std::string, std::string> defaults, options = flycast_profile::option_overrides();
    std::map<std::string, SnapshotInfo> snapshots;
    retro_hw_render_callback hw{};
    retro_system_av_info av{};
    flycast_profile::PendingValue<retro_system_av_info> pending_av;
    flycast_profile::PendingValue<retro_game_geometry> pending_geometry;
    bool hw_requested = false, rate_locked = false, input_started = false;
    std::atomic<bool> shutdown{false};
    std::atomic<const char*> callback_failure{nullptr};
    uint64_t audio_frames = 0, input_audio_origin = 0, polls = 0, a_edges = 0, start_edges = 0;
    flycast_profile::InputScript input_script;
    flycast_profile::Buttons buttons{};
    flycast_profile::FrameCounts video;
    unsigned last_width = 0, last_height = 0;
    std::atomic<unsigned> log_lines{0};
    double sample_rate = 0;
    std::string gl_version, renderer, vendor, egl_version;
    FILE* json = nullptr;
    void* gles_library = nullptr;
    double native_seconds() const {
        if (!input_started || !flycast_profile::valid_audio_rate(sample_rate)) return 0;
        return static_cast<double>(audio_frames - input_audio_origin) / sample_rate;
    }
};
State* state = nullptr;

void core_log(enum retro_log_level level, const char* format, ...) noexcept __attribute__((format(printf, 2, 3)));
void core_log(enum retro_log_level level, const char* format, ...) noexcept {
    // Keep error messages after the diagnostic's ordinary log budget is reached.
    if (state) {
        const unsigned line = state->log_lines.fetch_add(1, std::memory_order_relaxed);
        if (line >= 2000 || (line >= 400 && level < RETRO_LOG_WARN)) return;
    }
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

void put_string(FILE* out, const std::string& value) {
    std::fputc('"', out);
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { std::fputc('\\', out); std::fputc(c, out); }
        else if (c < 32) std::fprintf(out, "\\u%04x", static_cast<unsigned>(c));
        else std::fputc(c, out);
    }
    std::fputc('"', out);
}

void update_av(const retro_system_av_info& av) {
    if (!flycast_profile::valid_audio_rate(av.timing.sample_rate) || !std::isfinite(av.timing.fps) ||
        av.timing.fps < 10 || av.timing.fps > 240) {
        state->callback_failure = "invalid_audio_rate_or_target_refresh";
        return;
    }
    if (state->rate_locked && av.timing.sample_rate != state->sample_rate) {
        state->callback_failure = "audio_rate_changed_after_input_clock_started";
        return;
    }
    if (av.geometry.base_width > surface_size || av.geometry.base_height > surface_size) {
        state->callback_failure = "geometry_exceeds_offscreen_surface";
        return;
    }
    state->av = av;
    state->sample_rate = av.timing.sample_rate;
}

void apply_pending_av() {
    // SPG calls SET_SYSTEM_AV_INFO on its emulation worker. Only this caller
    // thread may change the sample rate used by audio gates and input polling.
    retro_system_av_info av{};
    if (state->pending_av.take(av)) update_av(av);
    retro_game_geometry geometry{};
    if (state->pending_geometry.take(geometry)) {
        state->av.geometry = geometry;
        if (geometry.base_width > surface_size || geometry.base_height > surface_size)
            state->callback_failure = "geometry_exceeds_offscreen_surface";
    }
}

uintptr_t current_framebuffer() noexcept { return 0; }
retro_proc_address_t proc_address(const char* name) noexcept {
    auto result = eglGetProcAddress(name);
    if (!result && state && state->gles_library)
        result = reinterpret_cast<__eglMustCastToProperFunctionPointerType>(dlsym(state->gles_library, name));
    return reinterpret_cast<retro_proc_address_t>(result);
}

void add_v1(const retro_core_option_definition* definitions) {
    if (!definitions) return;
    for (size_t i = 0; i < 4096 && definitions[i].key; ++i) {
        const auto& d = definitions[i];
        if (d.default_value) state->defaults[d.key] = d.default_value;
        else if (d.values[0].value) state->defaults[d.key] = d.values[0].value;
    }
}
void add_v2(const retro_core_options_v2* definitions) {
    if (!definitions || !definitions->definitions) return;
    for (size_t i = 0; i < 4096 && definitions->definitions[i].key; ++i) {
        const auto& d = definitions->definitions[i];
        if (d.default_value) state->defaults[d.key] = d.default_value;
        else if (d.values[0].value) state->defaults[d.key] = d.values[0].value;
    }
}

bool environment(unsigned command, void* data) noexcept {
    if (!state) return false;
    try {
        switch (command) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            if (!data) return false;
            static_cast<retro_log_callback*>(data)->log = core_log; return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            if (!data) return false;
            *static_cast<const char**>(data) = state->system_dir.c_str(); return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            if (!data) return false;
            *static_cast<const char**>(data) = state->save_dir.c_str(); return true;
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            if (!data) return false;
            *static_cast<unsigned*>(data) = 2; return true;
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: add_v2(static_cast<retro_core_options_v2*>(data)); return true;
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
            if (!data) return false;
            add_v2(static_cast<retro_core_options_v2_intl*>(data)->us); return true;
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: add_v1(static_cast<retro_core_option_definition*>(data)); return true;
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
            if (!data) return false;
            add_v1(static_cast<retro_core_options_intl*>(data)->us); return true;
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            if (!data) return false;
            const auto* variable = static_cast<retro_variable*>(data);
            for (size_t i = 0; i < 4096 && variable[i].key; ++i)
                if (variable[i].value) state->defaults[variable[i].key] = flycast_profile::legacy_default(variable[i].value);
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            if (!data) return false;
            auto* variable = static_cast<retro_variable*>(data);
            variable->value = nullptr;
            if (!variable->key) return false;
            auto it = state->options.find(variable->key);
            if (it != state->options.end()) variable->value = it->second.c_str();
            else {
                auto d = state->defaults.find(variable->key);
                if (d != state->defaults.end()) variable->value = d->second.c_str();
            }
            return variable->value != nullptr;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
            if (!data) return false;
            *static_cast<bool*>(data) = false; return true;
        case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
            if (!data) return false;
            *static_cast<unsigned*>(data) = RETRO_HW_CONTEXT_OPENGLES3; return true;
        case RETRO_ENVIRONMENT_SET_HW_RENDER: {
            if (!data) return false;
            auto* hw = static_cast<retro_hw_render_callback*>(data);
            if (hw->context_type != RETRO_HW_CONTEXT_OPENGLES2 && hw->context_type != RETRO_HW_CONTEXT_OPENGLES3 &&
                hw->context_type != RETRO_HW_CONTEXT_OPENGLES_VERSION) return false;
            hw->get_current_framebuffer = current_framebuffer;
            hw->get_proc_address = proc_address;
            state->hw = *hw;
            state->hw_requested = true;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return data && *static_cast<retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
            if (!data) return false;
            state->pending_av.publish(*static_cast<retro_system_av_info*>(data)); return true;
        case RETRO_ENVIRONMENT_SET_GEOMETRY:
            if (!data) return false;
            state->pending_geometry.publish(*static_cast<retro_game_geometry*>(data)); return true;
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            if (!data) return false;
            *static_cast<unsigned*>(data) = RETRO_LANGUAGE_ENGLISH; return true;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (!data) return false;
            *static_cast<bool*>(data) = true; return true;
        case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
            if (!data) return false;
            *static_cast<unsigned*>(data) = 4; return true;
        case RETRO_ENVIRONMENT_SHUTDOWN: state->shutdown = true; return true;
        case RETRO_ENVIRONMENT_SET_MESSAGE:
            if (data && static_cast<retro_message*>(data)->msg)
                core_log(RETRO_LOG_INFO, "message: %s\n", static_cast<retro_message*>(data)->msg);
            return true;
        case RETRO_ENVIRONMENT_SET_ROTATION:
            return data && *static_cast<unsigned*>(data) == 0;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
            return true;
        default: return false;
        }
    } catch (...) {
        state->callback_failure = "exception_in_environment_callback";
        return false;
    }
}

void video(const void* data, unsigned width, unsigned height, size_t) noexcept {
    if (!state) return;
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        state->video.observe(flycast_profile::FrameKind::fresh);
        if (!width || !height || width > surface_size || height > surface_size)
            state->callback_failure = "invalid_video_dimensions";
        else { state->last_width = width; state->last_height = height; }
    } else if (!data) state->video.observe(flycast_profile::FrameKind::duplicate);
    else {
        state->video.observe(flycast_profile::FrameKind::unexpected);
        state->callback_failure = "unexpected_software_video";
    }
}
size_t audio_batch(const int16_t*, size_t frames) noexcept {
    if (state) state->audio_frames += frames;
    return frames;
}
void audio_sample(int16_t, int16_t) noexcept { if (state) ++state->audio_frames; }
void input_poll() noexcept {
    if (!state) return;
    if (!state->input_started) {
        state->input_started = true;
        state->input_audio_origin = state->audio_frames;
        state->rate_locked = true;
    }
    const auto next = state->input_script.buttons(state->native_seconds());
    if (next.a && !state->buttons.a) ++state->a_edges;
    if (next.start && !state->buttons.start) ++state->start_edges;
    state->buttons = next;
    ++state->polls;
}
int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) noexcept {
    if (!state || port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0) return 0;
    // Flycast dc_joymap maps Retro B to DC A. Retro A instead maps to DC B.
    if (id == RETRO_DEVICE_ID_JOYPAD_B) return state->buttons.a;
    if (id == RETRO_DEVICE_ID_JOYPAD_START) return state->buttons.start;
    return 0;
}

void ensure_dir(const std::string& path) {
    if (mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) throw std::runtime_error("cannot_create_private_directory");
    struct stat info{};
    if (stat(path.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) throw std::runtime_error("private_path_is_not_directory");
}
std::string canonical(const std::string& path) {
    std::array<char, PATH_MAX> result{};
    if (!realpath(path.c_str(), result.data())) throw std::runtime_error("cannot_resolve_path");
    return result.data();
}

struct Runtime {
    Api api;
    void* core = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    bool inited = false, loaded = false, context_reset = false, cleanup_ok = true;

    template<class Function> void symbol(Function& function, const char* name) {
        function = reinterpret_cast<Function>(dlsym(core, name));
        if (!function) throw std::runtime_error(std::string("missing_core_symbol:") + name);
    }
    void load_core(const char* path) {
        core = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!core) throw std::runtime_error(std::string("core_dlopen:") + dlerror());
        symbol(api.version, "retro_api_version");
        symbol(api.set_environment, "retro_set_environment"); symbol(api.set_video, "retro_set_video_refresh");
        symbol(api.set_audio, "retro_set_audio_sample"); symbol(api.set_audio_batch, "retro_set_audio_sample_batch");
        symbol(api.set_poll, "retro_set_input_poll"); symbol(api.set_input, "retro_set_input_state");
        symbol(api.init, "retro_init"); symbol(api.deinit, "retro_deinit");
        symbol(api.system_info, "retro_get_system_info"); symbol(api.av_info, "retro_get_system_av_info");
        symbol(api.controller, "retro_set_controller_port_device"); symbol(api.load, "retro_load_game");
        symbol(api.unload, "retro_unload_game"); symbol(api.run, "retro_run");
        if (api.version() != RETRO_API_VERSION) throw std::runtime_error("libretro_api_version_mismatch");
    }
    void create_egl() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major = 0, minor = 0;
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor))
            throw std::runtime_error("egl_initialize_failed");
        state->egl_version = std::to_string(major) + "." + std::to_string(minor);
        if (!eglBindAPI(EGL_OPENGL_ES_API)) throw std::runtime_error("egl_bind_es_failed");
        const EGLint config_attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        if (!eglChooseConfig(display, config_attributes, &config, 1, &count) || count != 1)
            throw std::runtime_error("egl_no_rgba8_depth24_stencil8_es3_pbuffer_config");
        const EGLint pbuffer_attributes[] = {EGL_WIDTH, surface_size, EGL_HEIGHT, surface_size, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, pbuffer_attributes);
        const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
        if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context))
            throw std::runtime_error("egl_create_or_make_current_failed");
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        if (!version || !renderer || !vendor) throw std::runtime_error("missing_gl_identity");
        state->gl_version = version; state->renderer = renderer; state->vendor = vendor;
        GLint gl_major = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &gl_major);
        if (gl_major < 3) throw std::runtime_error("actual_context_is_not_gles3");
        if (state->renderer.find("llvmpipe") != std::string::npos || state->renderer.find("SwiftShader") != std::string::npos)
            throw std::runtime_error("software_renderer_rejected");
        state->gles_library = dlopen("libGLESv3.so", RTLD_NOW | RTLD_LOCAL);
        if (!state->gles_library) state->gles_library = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
        if (!state->gles_library) throw std::runtime_error("gles_symbol_library_failed");
        std::fprintf(stderr, "EGL %s; %s; renderer=%s; vendor=%s\n", state->egl_version.c_str(), version, renderer, vendor);
    }
    void cleanup() noexcept {
        // Core worker termination must precede destruction of its current GL context.
        try {
            if (loaded) { loaded = false; api.unload(); }
            if (context_reset && state->hw.context_destroy) {
                context_reset = false;
                state->hw.context_destroy();
            }
            if (inited) { inited = false; api.deinit(); }
        } catch (...) {
            state->callback_failure = "exception_during_core_cleanup";
            cleanup_ok = false;
            // A failed unload may leave a worker alive. Let process exit reclaim EGL
            // instead of destroying resources potentially still used by that worker.
            return;
        }
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        context = EGL_NO_CONTEXT; surface = EGL_NO_SURFACE; display = EGL_NO_DISPLAY;
        // Flycast owns signal handlers and thread-local/JIT state: leave modules mapped
        // until process exit rather than risking a dlclose while late TLS cleanup runs.
    }
};

void snapshot(const std::string& name) {
    if (!state->last_width || !state->last_height) throw std::runtime_error("snapshot_has_no_native_video");
    const unsigned width = state->last_width, height = state->last_height;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    GLint pack = 0, row = 0, skip_pixels = 0, skip_rows = 0, pbo = 0, framebuffer = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &pack); glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels); glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0); glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum error = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, pack); glPixelStorei(GL_PACK_ROW_LENGTH, row);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels); glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(pbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    if (error != GL_NO_ERROR) throw std::runtime_error("snapshot_gl_read_failed");
    FILE* file = std::fopen((state->root + "/" + name + ".rgba").c_str(), "wbx");
    if (!file) throw std::runtime_error("snapshot_open_failed");
    const bool complete = std::fwrite(pixels.data(), 1, pixels.size(), file) == pixels.size();
    const bool closed = std::fclose(file) == 0;
    if (!complete || !closed) throw std::runtime_error("snapshot_write_failed");
    state->snapshots[name + ".rgba"] = {width, height, state->native_seconds()};
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>((values.size() - 1) * fraction)];
}

struct Measurement {
    uint64_t runs = 0, fresh = 0, duplicates = 0, audio = 0;
    double wall = 0, gpu_wait_seconds = 0;
    std::vector<double> intervals_ms;
};

void write_report(const char* result, const std::string& failure, const Measurement& m, unsigned total_runs,
                  double total_wall, unsigned warmup, unsigned measure, unsigned max_wall, unsigned max_runs,
                  bool snapshots, bool warmup_snapshot, bool end_snapshot) {
    if (!state->json) return;
    FILE* out = state->json;
    std::fprintf(out, "{\n\"schema\":1,\"result\":"); put_string(out, result);
    std::fprintf(out, ",\"failure\":"); put_string(out, failure);
    std::fprintf(out, ",\n\"core\":"); put_string(out, state->library);
    std::fprintf(out, ",\"ownedCue\":"); put_string(out, state->cue);
    std::fprintf(out, ",\n\"eglVersion\":"); put_string(out, state->egl_version);
    std::fprintf(out, ",\"glVersion\":"); put_string(out, state->gl_version);
    std::fprintf(out, ",\"renderer\":"); put_string(out, state->renderer);
    std::fprintf(out, ",\"vendor\":"); put_string(out, state->vendor);
    std::fprintf(out, ",\n\"warmupNativeSeconds\":%u,\"measureNativeSeconds\":%u,\"maxWallSeconds\":%u,\"maxRuns\":%u,"
        "\"totalRuns\":%u,\"totalWallSeconds\":%.9f,\"audioRate\":%.3f,\"targetRefreshHz\":%.9f,"
        "\"totalFreshMain\":%llu,\"totalDuplicates\":%llu,\"unexpectedVideo\":%llu,\"totalAudioFrames\":%llu,"
        "\"inputPolls\":%llu,\"nativeAEdges\":%llu,\"nativeStartEdges\":%llu,\"inputAudioNativeSeconds\":%.9f,",
        warmup, measure, max_wall, max_runs, total_runs, total_wall, state->sample_rate, state->av.timing.fps,
        state->video.fresh, state->video.duplicates, state->video.unexpected,
        static_cast<unsigned long long>(state->audio_frames), static_cast<unsigned long long>(state->polls),
        static_cast<unsigned long long>(state->a_edges), static_cast<unsigned long long>(state->start_edges), state->native_seconds());
    const double audio_seconds = flycast_profile::valid_audio_rate(state->sample_rate) ? m.audio / state->sample_rate : 0;
    std::fprintf(out, "\n\"measurement\":{\"runs\":%llu,\"freshMain\":%llu,\"duplicates\":%llu,\"audioFrames\":%llu,"
        "\"wallSeconds\":%.9f,\"freshMainHz\":%.9f,\"audioDerivedNativeSeconds\":%.9f,"
        "\"audioDerivedNativeSpeedPct\":%.9f,\"gpuFinishWaitSeconds\":%.9f,\"runMsP10\":%.6f,\"runMsP50\":%.6f,\"runMsP90\":%.6f},",
        static_cast<unsigned long long>(m.runs), static_cast<unsigned long long>(m.fresh),
        static_cast<unsigned long long>(m.duplicates), static_cast<unsigned long long>(m.audio), m.wall,
        m.wall > 0 ? m.fresh / m.wall : 0, audio_seconds, m.wall > 0 ? audio_seconds / m.wall * 100 : 0,
        m.gpu_wait_seconds, percentile(m.intervals_ms, .1), percentile(m.intervals_ms, .5), percentile(m.intervals_ms, .9));
    std::fprintf(out, "\n\"inputClock\":\"delivered audio frames / reported sample rate from first input poll; proxy, buffer drops not observable\","
        "\"gpuCompletion\":\"glFinish after every retro_run; serialized CPU/GPU\","
        "\"workloadRequiresImageReview\":true,\"xrTested\":false,\"physicalPlayTested\":false,"
        "\"snapshotsRequested\":%s,\"warmupSnapshot\":%s,\"endSnapshot\":%s,\"snapshotWidth\":%u,\"snapshotHeight\":%u,"
        "\"snapshotFormat\":\"RGBA8 tightly packed; origin bottom-left; captures outside measurement\",\n\"options\":{",
        snapshots ? "true" : "false", warmup_snapshot ? "true" : "false", end_snapshot ? "true" : "false",
        state->last_width, state->last_height);
    auto effective = state->defaults;
    for (const auto& option : state->options) effective[option.first] = option.second;
    bool first = true;
    for (const auto& option : effective) {
        if (!first) std::fputc(',', out);
        first = false; put_string(out, option.first); std::fputc(':', out); put_string(out, option.second);
    }
    std::fprintf(out, "},\n\"snapshotFiles\":{");
    first = true;
    for (const auto& item : state->snapshots) {
        if (!first) std::fputc(',', out);
        first = false; put_string(out, item.first);
        std::fprintf(out, ":{\"width\":%u,\"height\":%u,\"audioDerivedNativeSeconds\":%.9f}",
            item.second.width, item.second.height, item.second.native_audio_seconds);
    }
    std::fprintf(out, "}\n}\n");
    if (std::fflush(out) != 0 || std::ferror(out)) throw std::runtime_error("report_write_failed");
}
}

int main(int argc, char** argv) {
    unsigned warmup = 35, measure = 10, max_wall = 90, max_runs = 100000;
    bool snapshots = false;
    if (argc < 4 || argc > 9 ||
        (argc > 4 && !flycast_profile::parse_unsigned(argv[4], 0, 600, warmup)) ||
        (argc > 5 && !flycast_profile::parse_unsigned(argv[5], 1, 300, measure)) ||
        (argc > 6 && !flycast_profile::parse_unsigned(argv[6], 1, 3600, max_wall)) ||
        (argc > 7 && !flycast_profile::parse_unsigned(argv[7], 1, 1000000, max_runs)) ||
        (argc > 8 && std::strcmp(argv[8], "--snapshots") != 0)) {
        std::fprintf(stderr, "Usage: %s CORE_LIBRARY OWNED_CUE PRIVATE_ROOT [warmupNativeSeconds=35] [measureNativeSeconds=10] [maxWallSeconds=90] [maxRuns=100000] [--snapshots]\n", argv[0]);
        return 2;
    }
    snapshots = argc > 8;
    State data;
    state = &data;
    Runtime runtime;
    Measurement measured;
    unsigned runs = 0;
    bool warmup_snapshot = false, end_snapshot = false, measuring = false, completed = false;
    std::string failure;
    const auto process_start = Clock::now();
    std::signal(SIGINT, on_signal); std::signal(SIGTERM, on_signal);
    try {
        if (!flycast_profile::valid_private_path(argv[3])) throw std::runtime_error("private_root_must_be_private_android_profile_path");
        ensure_dir(argv[3]);
        data.root = canonical(argv[3]);
        if (!flycast_profile::valid_private_path(data.root)) throw std::runtime_error("resolved_private_root_outside_profile_paths");
        data.json = std::fopen((data.root + "/profile.json").c_str(), "wx");
        if (!data.json) throw std::runtime_error("profile_json_exists_or_is_unwritable_use_fresh_private_root");
        data.library = canonical(argv[1]); data.cue = canonical(argv[2]);
        if (data.cue.size() < 4 || data.cue.substr(data.cue.size() - 4) != ".cue") throw std::runtime_error("owned_content_must_be_cue");
        data.system_dir = data.root + "/system"; data.save_dir = data.root + "/saves";
        for (const auto& path : {data.system_dir, data.system_dir + "/dc", data.save_dir}) {
            ensure_dir(path);
            if (canonical(path).compare(0, data.root.size() + 1, data.root + "/") != 0)
                throw std::runtime_error("private_directory_symlink_escapes_root");
        }
        // Remove diagnostic environment opt-ins from this child process only.
        unsetenv("HOTD2VR_STALLRENDER");
        runtime.create_egl();
        runtime.load_core(data.library.c_str());
        runtime.api.set_environment(environment); runtime.api.set_video(video);
        runtime.api.set_audio(audio_sample); runtime.api.set_audio_batch(audio_batch);
        runtime.api.set_poll(input_poll); runtime.api.set_input(input_state);
        runtime.inited = true; runtime.api.init();
        retro_system_info info{}; runtime.api.system_info(&info);
        std::fprintf(stderr, "Core=%s revision=%s fullpath=%d\n", info.library_name ? info.library_name : "?",
            info.library_version ? info.library_version : "?", info.need_fullpath);
        for (unsigned port = 0; port < 4; ++port)
            runtime.api.controller(port, port == 0 ? RETRO_DEVICE_JOYPAD : RETRO_DEVICE_NONE);
        retro_game_info game{}; game.path = data.cue.c_str();
        if (!runtime.api.load(&game)) throw std::runtime_error("retro_load_game_failed");
        runtime.loaded = true;
        retro_system_av_info av{}; runtime.api.av_info(&av); update_av(av); apply_pending_av();
        if (data.callback_failure) throw std::runtime_error(data.callback_failure.load());
        if (!data.hw_requested || !data.hw.context_reset) throw std::runtime_error("core_did_not_negotiate_gl_context");
        for (const auto& option : data.options)
            if (data.defaults.find(option.first) == data.defaults.end())
                throw std::runtime_error("required_core_option_not_declared:" + option.first);
        runtime.context_reset = true;
        data.hw.context_reset();
        glFinish();
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("gl_context_reset_error");
        Clock::time_point measurement_start{}, last_progress = Clock::now();
        uint64_t start_audio = 0, start_fresh = 0, start_duplicates = 0;
        while (runs < max_runs) {
            apply_pending_av();
            if (elapsed(process_start) >= max_wall) throw std::runtime_error("max_wall_before_native_measurement_completed");
            if (stop_requested) throw std::runtime_error("interrupted");
            if (data.shutdown) throw std::runtime_error("core_requested_shutdown");
            if (data.callback_failure) throw std::runtime_error(data.callback_failure.load());
            const auto before = Clock::now();
            runtime.api.run();
            ++runs;
            apply_pending_av();
            const auto finish_start = Clock::now();
            glFinish();
            const double finish_wait = elapsed(finish_start);
            const double run_ms = elapsed(before) * 1000;
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("gl_error_after_retro_run");
            if (data.callback_failure) throw std::runtime_error(data.callback_failure.load());
            if (elapsed(process_start) > 12 && data.audio_frames == 0) throw std::runtime_error("no_audio_native_clock_unavailable");
            if (measuring) {
                ++measured.runs;
                measured.gpu_wait_seconds += finish_wait;
                measured.intervals_ms.push_back(run_ms);
                measured.audio = data.audio_frames - start_audio;
                measured.fresh = data.video.fresh - start_fresh;
                measured.duplicates = data.video.duplicates - start_duplicates;
                measured.wall = elapsed(measurement_start);
                if (flycast_profile::audio_gate(measured.audio, data.sample_rate, measure)) { completed = true; break; }
            } else if (data.input_started && data.native_seconds() >= warmup) {
                if (snapshots) {
                    snapshot("warmup"); warmup_snapshot = true;
                    // The emulation worker may progress while the image is written.
                    // Flush its queued audio outside the measurement boundary.
                    if (runs >= max_runs || elapsed(process_start) >= max_wall)
                        throw std::runtime_error("budget_exhausted_after_warmup_snapshot");
                    runtime.api.run(); ++runs; apply_pending_av(); glFinish();
                    if (glGetError() != GL_NO_ERROR || data.callback_failure || data.shutdown)
                        throw std::runtime_error("warmup_snapshot_audio_flush_failed");
                }
                glFinish();
                start_audio = data.audio_frames; start_fresh = data.video.fresh; start_duplicates = data.video.duplicates;
                measurement_start = Clock::now();
                measuring = true;
            }
            if (elapsed(last_progress) >= 5) {
                std::fprintf(stderr, "progress wall=%.2f nativeAudio=%.2f runs=%u fresh=%llu dupes=%llu measuring=%d\n",
                    elapsed(process_start), data.native_seconds(), runs, data.video.fresh, data.video.duplicates, measuring);
                last_progress = Clock::now();
            }
        }
        if (!completed) throw std::runtime_error("max_runs_before_native_measurement_completed");
        if (!measured.audio || !measured.fresh || data.video.unexpected) throw std::runtime_error("no_valid_completed_native_workload");
        if (data.start_edges != 1 && warmup >= 30) throw std::runtime_error("input_recipe_did_not_issue_exactly_one_start");
        if (snapshots) { snapshot("end"); end_snapshot = true; }
    } catch (const std::exception& error) { failure = error.what(); }
      catch (...) { failure = "unknown_frontend_or_core_exception"; }
    runtime.cleanup();
    if (data.callback_failure && failure.empty()) failure = data.callback_failure.load();
    if (failure.empty() && !completed) failure = "measurement_incomplete";
    try {
        write_report(failure.empty() ? "measurement_complete_requires_workload_review" : "blocked", failure, measured,
            runs, elapsed(process_start), warmup, measure, max_wall, max_runs, snapshots, warmup_snapshot, end_snapshot);
    } catch (const std::exception& error) { failure = error.what(); }
    if (data.json && std::fclose(data.json) != 0) failure = "report_close_failed";
    std::fprintf(stderr, "result=%s failure=%s runs=%u measuredFresh=%llu wall=%.6f\n",
        failure.empty() ? "complete_requires_image_review" : "blocked", failure.c_str(), runs,
        static_cast<unsigned long long>(measured.fresh), measured.wall);
    if (!runtime.cleanup_ok) {
        std::fflush(stderr);
        std::_Exit(1); // do not destroy callback state while a failed-unload worker might still exist
    }
    state = nullptr;
    return failure.empty() ? 0 : 1;
}
