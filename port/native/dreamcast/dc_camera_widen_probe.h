// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_camera_probe.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
struct Sh4Context;
namespace silent_scope { namespace dreamcast {
struct CameraWidenDecision {uint32_t effective_bits{};bool eligible{},applied{};};
// Diagnostic only: factor1 is the timing-matched control, factor3 is the trial.
// Constant bit results avoid modifying host floating status or guest FPSCR.
class CameraWidenProbe {
    int factor_{};
public:
    static constexpr uint32_t TargetPc=0x8c019002,SourceAddress=0x8c1b0108;
    static constexpr uint16_t OriginalOpcode=0xf338,TrapOpcode=0x085b;
    static constexpr std::array<std::pair<uint32_t,uint16_t>,13> SetupGuard{{
        {0x8c018fbc,0xd161},{0x8c018fc0,0xe200},{0x8c018fc6,0x2122},
        {0x8c018ff8,0xd358},{0x8c018ffa,0xff37},{0x8c018ffc,0xe024},
        {0x8c018ffe,0xff47},{0x8c019000,0xe014},{0x8c019004,0x64f3},
        {0x8c019006,0xd256},{0x8c019008,0xff37},{0x8c01900a,0x420b},
        {0x8c01900c,0x7414}}};
    explicit CameraWidenProbe(int factor=0):factor_(factor==1||factor==3?factor:0){}
    bool enabled()const{return factor_!=0;}
    int factor()const{return factor_;}
    template<class Read32,class Read16> static bool guarded(const std::string& title,bool trap,Read32 read32,Read16 read16){
        if(title!="T9507N")return false;
        if(read16(TargetPc)!=(trap?TrapOpcode:OriginalOpcode))return false;
        for(auto entry:CameraSampler::CodeGuard)if(read32(entry.first)!=entry.second)return false;
        for(auto entry:SetupGuard)if(read16(entry.first)!=entry.second)return false;
        return read32(0x8c01915c)==SourceAddress&&read32(0x8c019160)==0x8c112864&&read32(0x8c019144)==0x8c1b0114;
    }
    CameraWidenDecision evaluate(double nativeSeconds,uint32_t loadedBits,uint32_t r3,uint32_t fpscr,uint32_t mode,bool guard)const{
        CameraWidenDecision out{loadedBits,false,false};
        out.eligible=enabled()&&guard&&r3==SourceAddress&&loadedBits==0x440ac000&&mode==0
            &&(fpscr&((1u<<19)|(1u<<20)))==0;
        out.applied=out.eligible&&factor_==3&&std::isfinite(nativeSeconds)&&nativeSeconds>=45&&nativeSeconds<55;
        if(out.applied)out.effective_bits=0x43390000; //555/3=185 exactly
        return out;
    }
};
// Implemented in the diagnostic vr_reproject adapter. It replays the original
// FMOV through Flycast's interpreter and continues at the following instruction.
bool handle_camera_widen_trap(Sh4Context* ctx,uint32_t guestPc);
}}
