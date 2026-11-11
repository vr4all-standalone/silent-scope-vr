#pragma once
#include <map>
#include <string>
#include <cmath>
#include <mutex>
namespace flycast_profile {
template<class T> class PendingValue {
    std::mutex mutex;
    T value{};
    bool pending = false;
public:
    void publish(const T& next) {
        std::lock_guard<std::mutex> lock(mutex);
        value = next;
        pending = true;
    }
    bool take(T& out) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!pending) return false;
        out = value;
        pending = false;
        return true;
    }
};
inline bool parse_unsigned(const char* s, unsigned low, unsigned high, unsigned& out) {
    if (!s || !*s) return false;
    unsigned n = 0;
    for (; *s; ++s) {
        if (*s < '0' || *s > '9') return false;
        const unsigned digit = static_cast<unsigned>(*s - '0');
        if (digit > high || n > (high - digit) / 10) return false;
        n = n * 10 + digit;
    }
    if (n < low) return false;
    out = n;
    return true;
}
struct Buttons { bool a = false; bool start = false; };
struct InputScript {
    bool start_sent = false;
    bool start_held = false;
    double release_start = 0;
    Buttons buttons(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds >= 30) return {};
        if (seconds >= 20 && seconds < 21 && !start_sent) {
            start_sent = true;
            start_held = true;
            release_start = seconds + .2;
        }
        const bool a_window = seconds < 15 || (seconds >= 21 && seconds < 28);
        if (seconds >= release_start) start_held = false;
        return {a_window && std::fmod(seconds, 1.0) < .12,
                start_held};
    }
};
enum class FrameKind { fresh, duplicate, unexpected };
struct FrameCounts {
    unsigned long long fresh = 0, duplicates = 0, unexpected = 0;
    void observe(FrameKind kind) {
        if (kind == FrameKind::fresh) ++fresh;
        else if (kind == FrameKind::duplicate) ++duplicates;
        else ++unexpected;
    }
};
inline std::map<std::string, std::string> option_overrides() {
    return {{"reicast_hle_bios", "enabled"}, {"reicast_enable_rttb", "enabled"},
        {"reicast_enable_dsp", "enabled"}, {"reicast_internal_resolution", "640x480"},
        {"reicast_threaded_rendering", "enabled"}, {"reicast_auto_skip_frame", "disabled"},
        {"reicast_frame_skipping", "disabled"}, {"reicast_sh4clock", "200"},
        {"reicast_alpha_sorting", "per-triangle (normal)"},
        {"reicast_gdrom_fast_loading", "disabled"}, {"reicast_custom_textures", "disabled"},
        {"reicast_preload_custom_textures", "disabled"}, {"reicast_dump_textures", "disabled"},
        {"reicast_widescreen_hack", "disabled"}, {"reicast_widescreen_cheats", "disabled"},
        {"reicast_emulate_framebuffer", "disabled"}, {"reicast_detect_vsync_swap_interval", "disabled"}};
}
inline bool valid_private_path(const std::string& path) {
    if (path.find("/../") != std::string::npos || path.find("/./") != std::string::npos ||
        path.find('\\') != std::string::npos || path.find('\n') != std::string::npos ||
        path.size() > 1800 || path.size() < 18 || path.back() == '/') return false;
    if (path.compare(0, 16, "/data/local/tmp/") == 0) return path.size() > 16;
    const bool app = path.compare(0, 13, "/data/user/0/") == 0 || path.compare(0, 11, "/data/data/") == 0;
    const auto files = path.find("/files/");
    return app && files != std::string::npos && files + 7 < path.size();
}
inline std::string legacy_default(const std::string& value) {
    const auto semicolon = value.find(';');
    if (semicolon == std::string::npos) return {};
    const auto start = value.find_first_not_of(' ', semicolon + 1);
    if (start == std::string::npos) return {};
    return value.substr(start, value.find('|', start) - start);
}
inline bool valid_audio_rate(double rate) {
    return std::isfinite(rate) && rate >= 8000 && rate <= 192000;
}
inline bool audio_gate(unsigned long long frames, double rate, double seconds) {
    return valid_audio_rate(rate) && static_cast<double>(frames) >= seconds * rate;
}
}
