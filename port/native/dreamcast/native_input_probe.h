// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Opt-in desktop boot/menu diagnostic, not a VR input adapter. Native Maple
// response overlays only: no emulated RAM, device configuration or clock edits.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string_view>

namespace silent_scope { namespace dc_input_probe {
enum class Mode { Disabled, PadStart, MouseStart };
enum class Device { Other, Pad, Mouse };
// Flycast 8398fead core/input/gamepad.h:22-25; active-low Maple kcode.
constexpr std::uint32_t PadA=1u<<2,PadStart=1u<<3;
// core/input/mouse.h:70-73 LEFT_BUTTON=2; mouse.cpp:62-69 clears
// (1<<button) when pressed. This is the native mouse bit, not a pad mapping.
constexpr std::uint8_t MouseLeft=1u<<2;
inline Mode parse_mode(const char* value) noexcept {
    if(!value)return Mode::Disabled;
    const std::string_view text(value);
    if(text=="pad-start")return Mode::PadStart;
    if(text=="mouse-start")return Mode::MouseStart;
    return Mode::Disabled;
}
struct Sample {
    bool owned{};
    std::uint32_t pad_pressed{};
    std::uint8_t mouse_pressed{};
};
class Policy {
public:
    explicit Policy(Mode mode=Mode::Disabled) noexcept:mode_(mode){}
    Sample poll(std::string_view product,Device device,unsigned player,double now) noexcept {
        if(mode_==Mode::Disabled || cancelled_)return {};
        if(product!="T9507N") {
            if(started_)cancelled_=true;
            return {};
        }
        const bool own=player==0 && ((mode_==Mode::PadStart && device==Device::Pad)
                                   || (mode_==Mode::MouseStart && device==Device::Mouse));
        if(!own || !std::isfinite(now) || now<0)return {};
        if(!started_){start_=last_=now;started_=true;}
        // A reversed/invalid time sample never replays earlier events. There is
        // no deferred button queue: a missed native poll simply misses a pulse.
        if(now<last_)return {true,0,0};
        last_=now;
        const double elapsed=now-start_;
        if(elapsed>=30)return {true,0,0};
        const bool warning=elapsed<15;
        const bool choice=mode_==Mode::PadStart ? elapsed>=21 && elapsed<28
                                               : elapsed>=20 && elapsed<28;
        const bool pulse=(warning || choice) && now<start_+std::floor(elapsed)+.1;
        if(mode_==Mode::MouseStart)return {true,0,std::uint8_t(pulse?MouseLeft:0)};
        const bool start=elapsed>=20 && now<start_+20.2;
        return {true,start?PadStart:(pulse?PadA:0),0};
    }
private:
    Mode mode_;bool started_{},cancelled_{};double start_{},last_{};
};
inline bool apply_pad(const Sample& value,std::uint32_t& kcode) noexcept {
    if(!value.owned)return false;
    kcode&=~value.pad_pressed;
    return true;
}
inline bool apply_mouse(const Sample& value,std::uint8_t& buttons,int& x,int& y,int& wheel) noexcept {
    if(!value.owned)return false;
    buttons=std::uint8_t(buttons & ~value.mouse_pressed);
    x=y=wheel=0; // Relative aiming/sensitivity has not been characterized.
    return true;
}
// Environment values are sampled once; changing them cannot restart the finite
// schedule. One process owns one probe. Optional trace is bounded to 64 native
// state transitions, metadata only, written to the existing stderr stream.
inline Sample environment_sample(std::string_view product,Device device,unsigned player) noexcept {
    try {
        struct Runtime {
            Policy policy{parse_mode(std::getenv("SILENT_SCOPE_DC_INPUT_PROBE"))};
            std::mutex mutex;
            bool trace{[] {const char* value=std::getenv("SILENT_SCOPE_DC_INPUT_PROBE_TRACE");
                           return value && std::string_view(value)=="1";}()};
            Sample previous{};unsigned records{};
        };
        static Runtime runtime;
        const std::lock_guard<std::mutex> guard(runtime.mutex);
        const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const Sample value=runtime.policy.poll(product,device,player,now);
        if(value.owned && runtime.trace && runtime.records<64 &&
           (!runtime.previous.owned || value.pad_pressed!=runtime.previous.pad_pressed
                                    || value.mouse_pressed!=runtime.previous.mouse_pressed)) {
            std::fprintf(stderr,"SILENT_SCOPE_DC_INPUT_PROBE transition=%u monotonic=%.6f pad=%u mouse=%u\n",
                         runtime.records++,now,value.pad_pressed,unsigned(value.mouse_pressed));
        }
        if(value.owned)runtime.previous=value;
        return value;
    } catch(...) {return {};}
}
}} // namespace silent_scope::dc_input_probe
