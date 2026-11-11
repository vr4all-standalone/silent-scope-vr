// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <thread>
#include <array>

namespace silent_scope::dc_scope {
double monotonic_seconds() noexcept;
struct Source {
    const char* product{};
    bool is_rtt{}, native_copy{};
    unsigned width{}, height{}, destination{}, pack_mode{}, stride{};
    int clip_x{}, clip_y{}, clip_width{}, clip_height{};
    uint64_t submit_sequence{}, native_generation{};
    unsigned source_framebuffer{};
};
bool is_native_scope(const Source&) noexcept;

struct Target { unsigned texture{}, framebuffer{}; };
struct CopyResult { bool copied{}; unsigned gl_error{}; };
struct CaptureStatus {
    uint64_t attempts{},copied{},failed{},last_submit{};
    double last_seconds{-1};
};
// Functions execute only on the original render thread in its current GL context.
// create/copy restore every GL binding/enable they change. No CPU pixel transfer.
struct GlApi {
    Target (*create)(){};
    CopyResult (*copy)(Target,unsigned source_framebuffer){};
    void (*destroy)(Target){};
};
struct TextureView {
    unsigned texture{}, width{}, height{};
    uint64_t sequence{}, submit_sequence{}, native_generation{}, lifecycle_generation{};
    uintptr_t context{};
    double captured_seconds{};
    bool origin_bottom_left{true};
    // Native Dreamcast RTT projection puts upright image top at texture v0.
    // This differs from the GL storage-origin name above: do not flip this feed.
    bool content_top_at_v0{true};
    explicit operator bool() const noexcept { return texture != 0; }
};

class ScopeGpu {
public:
    ScopeGpu() = default;
    ScopeGpu(const ScopeGpu&) = delete;
    ScopeGpu& operator=(const ScopeGpu&) = delete;
    // Call before any capture, with a nonzero caller-owned context identity.
    // Reinitialization requires release/context_lost of the previous identity.
    bool initialize(GlApi, uintptr_t context) noexcept;
    void set_physical_rifle_active(bool) noexcept;
    void invalidate() noexcept; // thread-safe native reset/state-load notification
    bool capture(const Source&, uintptr_t context, double now_seconds) noexcept;
    TextureView acquire(uint64_t main_submit_sequence, uint64_t native_generation,
                        uintptr_t context, double now_seconds) const noexcept;
    bool release(uintptr_t context) noexcept;
    void context_lost() noexcept; // render thread; forget names without GL calls
    unsigned last_gl_error() const noexcept { return last_error_; }
    CaptureStatus capture_status() const noexcept { return status_; }
private:
    GlApi api_{};
    // Quest can finish two newer native RTTs before presenting their main
    // frames. Retain the compatible displayed image as well as queued copies.
    // Four256-squareRGBA images use1MiB; acquire keeps the same age/gap gates.
    std::array<Target,4> targets_{};
    std::array<TextureView,4> images_{};
    unsigned next_slot_{};
    uint64_t native_generation_{};
    std::thread::id owner_{};
    uintptr_t context_{};
    uint64_t sequence_{};
    std::atomic<uint64_t> lifecycle_{1};
    std::atomic<bool> active_{false};
    unsigned last_error_{};
    CaptureStatus status_{};
    bool owns(uintptr_t) const noexcept;
};
// One owner in the engine library. Views are borrowed until next capture/reset;
// the rifle must neither delete this texture nor retain it across contexts.
ScopeGpu& gpu_scope() noexcept;
#if defined(_WIN32)
#define SS_DC_SCOPE_API __declspec(dllexport)
#else
#define SS_DC_SCOPE_API __attribute__((visibility("default")))
#endif
extern "C" SS_DC_SCOPE_API void silent_scope_dc_scope_active(bool active) noexcept;
extern "C" SS_DC_SCOPE_API void silent_scope_dc_scope_invalidate() noexcept;
extern "C" SS_DC_SCOPE_API bool silent_scope_dc_scope_view(uint64_t main_submit_sequence,uint64_t native_generation,
    uintptr_t context,double now_seconds,TextureView* out) noexcept;
#undef SS_DC_SCOPE_API
}
