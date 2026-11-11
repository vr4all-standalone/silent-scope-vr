// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
namespace silent_scope { namespace dc_input {
struct AimLimits {int yaw{5500},pitch{4300};};
inline AimLimits limits_for_factor(double factor) noexcept {
    if(factor!=3)return {};
    constexpr double radians=6.2831853071795864769/65536.;
    return {int(std::lround(std::atan(std::tan(5500*radians)*factor)/radians)),
        int(std::lround(std::atan(std::tan(4300*radians)*factor)/radians))};
}
// Four locally owned clamp literals, plus every instruction in their native
// comparison/write sequence. This changes the native range, never aim state.
class AimLimitPatch {
    bool owned_{};
    static constexpr std::array<uint16_t,23> guards_{{
        0x902f,0x9430,0x03ed,0x3347,0x8b00,0x0e45,0x03ed,0x942b,
        0x3343,0x8900,0x0e45,0x9025,0x9427,0x03ed,0x3347,0x8b00,
        0x0e45,0x03ed,0x9422,0x3343,0x8900,0x0e45,0x4f26}};
    static std::array<uint16_t,4> words(bool wider) noexcept {
        const auto v=limits_for_factor(wider?3:1);
        return {{uint16_t(v.yaw),uint16_t(-v.yaw),uint16_t(v.pitch),uint16_t(-v.pitch)}};
    }
public:
    template<class Read> bool guard_code(std::string_view title,Read read) const {
        if(title!="T9507N")return false;
        for(unsigned i=0;i<guards_.size();++i)if(read(0x8c073e7eu+i*2)!=guards_[i])return false;
        if(read(0x8c073ee0)!=0x06fe||read(0x8c073ee2)!=0x06fc)return false;
        return true;
    }
    // Load/reset resynchronization reads only. Recognize all four exact stock
    // OR wide literals under the full code guard; never claim unknown memory.
    template<class Read> bool adopt(std::string_view title,Read read) {
        if(!guard_code(title,read))return false;
        for(bool wider:{false,true}){
            const auto expected=words(wider);bool matches=true;
            for(unsigned i=0;i<expected.size();++i)matches=matches&&read(0x8c073ee4u+i*2)==expected[i];
            if(matches){owned_=wider;return true;}
        }
        return false;
    }
    template<class Read> bool guard(std::string_view title,Read read) const {
        if(!guard_code(title,read))return false;
        const auto expected=words(owned_);
        for(unsigned i=0;i<expected.size();++i)if(read(0x8c073ee4u+i*2)!=expected[i])return false;
        return true;
    }
    // Call only on the SH4 thread, with the independently acknowledged camera
    // mode. False means no writes and no wider-range capability acknowledgement.
    template<class Read,class Write> bool apply(std::string_view title,bool wider,Read read,Write write) {
        if(!guard(title,read))return false;
        if(wider!=owned_){const auto next=words(wider);
            for(unsigned i=0;i<next.size();++i)write(0x8c073ee4u+i*2,next[i]);
            owned_=wider;
        }
        return true;
    }
    bool wider()const noexcept{return owned_;}
    void forget()noexcept{owned_=false;} // New guest/reset only; never restores unknown memory.
};
}}
