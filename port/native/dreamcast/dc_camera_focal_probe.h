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
struct FocalSetterDecision {uint32_t effective_bits{};bool eligible{},applied{};};
class FocalSetterProbe {
    int factor_{};
public:
    static constexpr uint32_t TargetPc=0x8c112854,CallerReturn=0x8c019422,SourceAddress=0x8c1b0108;
    static constexpr uint16_t OriginalOpcode=0xd201,StoreOpcode=0xf24a,TrapOpcode=0x085b;
    static constexpr std::array<std::pair<uint32_t,uint16_t>,9> HalfGuard{{
        {0x8c112856,0x000b},{0x8c112858,0xf24a},{0x8c019416,0xd33b},
        {0x8c019418,0xd23b},{0x8c01941a,0x6032},{0x8c01941c,0x4008},
        {0x8c01941e,0x420b},{0x8c019420,0xf4e6},{0x8c019002,0xf338}}};
    static constexpr std::array<std::pair<uint32_t,uint32_t>,4> WordGuard{{
        {0x8c11285c,0x8cd1f9c0},{0x8c019504,0x8c1b0114},
        {0x8c019508,0x8c112854},{0x8c019398,0x8c1b0108}}};
    explicit FocalSetterProbe(int factor=0):factor_(factor==1||factor==3?factor:0){}
    bool enabled()const{return factor_!=0;}
    int factor()const{return factor_;}
    template<class Read32,class Read16>static bool guarded(const std::string& title,bool trap,Read32 read32,Read16 read16){
        if(title!="T9507N"||read16(TargetPc)!=(trap?TrapOpcode:OriginalOpcode))return false;
        for(auto g:CameraSampler::CodeGuard)if(read32(g.first)!=g.second)return false;
        for(auto g:HalfGuard)if(read16(g.first)!=g.second)return false;
        for(auto g:WordGuard)if(read32(g.first)!=g.second)return false;
        return true;
    }
    FocalSetterDecision evaluate(double now,uint32_t loaded,uint32_t caller,uint32_t mode,uint32_t fpscr,bool guard,bool fpu_disabled=false)const{
        FocalSetterDecision result{loaded,false,false};
        result.eligible=enabled()&&guard&&!fpu_disabled&&caller==CallerReturn&&mode==0&&loaded==0x440ac000
            &&(fpscr&((1u<<19)|(1u<<20)))==0;
        result.applied=result.eligible&&factor_==3&&std::isfinite(now)&&now>=45&&now<55;
        if(result.applied)result.effective_bits=0x43390000;
        return result;
    }
};
template<class Context>class CameraFocalRegisterRestore {
    Context* ctx_;uint32_t original_;
public:
    CameraFocalRegisterRestore(Context* ctx,uint32_t effective):ctx_(ctx),original_(ctx->fr_hex(4)){ctx_->fr_hex(4)=effective;}
    ~CameraFocalRegisterRestore(){ctx_->fr_hex(4)=original_;}
};
bool handle_camera_focal_trap(Sh4Context* ctx,uint32_t guestPc);
}}
