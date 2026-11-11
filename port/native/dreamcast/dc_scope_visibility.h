// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_scope_camera.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <utility>
namespace silent_scope::dreamcast {
struct ScopeVisibilitySite {
    static constexpr uint32_t TargetPc=0x8c058b66,Frustum=0x8c1f7554;
    static constexpr uint16_t OriginalOpcode=0x85ec,TrapOpcode=0x085b;
    static constexpr std::array<std::pair<uint32_t,uint32_t>,7> WordGuard{{
        {0x8c058b5c,0x430b56f8},{0x8c058b60,0xba5d54f5},
        {0x8c058b68,0x6503d340},{0x8c058b6c,0x640385ed},
        {0x8c058c60,Frustum},{0x8c058c74,0x8c059cc0},{0x8c058c80,0x8c06c3d8}}};
    template<class R32,class R16>static bool guarded(bool trap,R32 read32,R16 read16){
        if(read16(TargetPc)!=(trap?TrapOpcode:OriginalOpcode)||read16(0x8c058b64)!=0x0009||
            read16(0x8c058bc8)!=0x1ec5)return false;
        for(auto g:WordGuard)if(read32(g.first)!=g.second)return false;
        return true;
    }
};
struct ScopeVisibilityFacts {
    uint32_t mode{},frustum{},live_return{},outer_return{},original_span{},scope_source{},sdk_focal{};
    std::array<int16_t,4> planes{};
    bool code_guard{};
};
struct ScopeVisibilityDecision {std::array<int16_t,4> planes{};bool applied{};float scale{1};};
inline ScopeVisibilityDecision decide_scope_visibility(const ScopeVisibilityFacts& f){
    ScopeVisibilityDecision out{f.planes,false,1};
    if(!f.code_guard||f.mode!=1||f.frustum!=ScopeVisibilitySite::Frustum||
        f.live_return!=ScopeVisibilitySite::TargetPc||
        (f.outer_return!=0x8c074898&&f.outer_return!=0x8c0748e8)||
        f.original_span!=200||!known_scope_world_focal(f.scope_source))return out;
    // Use the SDK focal actually applied to this native camera, not a newer XR
    // preference that could change between projection and visibility setup.
    float stock;std::memcpy(&stock,&f.scope_source,4);
    for(float scale:{.2f,.5f}){
        const float focal=stock*scale;uint32_t bits;std::memcpy(&bits,&focal,4);
        if(bits==f.sdk_focal)out.scale=scale;
    }
    if(out.scale==1)return out;
    constexpr double amplitude=16384;
    for(int axis:{0,2}){
        const double sine=f.planes[axis],cosine=f.planes[axis+1];
        const double norm=sine*sine+cosine*cosine;
        if(sine<=0||sine>=8192||cosine<8192||cosine>amplitude||
            norm<16382.*16382.||norm>amplitude*amplitude)return {f.planes,false,1};
        const double tangent=sine/cosine/out.scale;
        const int s=int(std::ceil(amplitude*tangent/std::sqrt(1+tangent*tangent)));
        int c=int(std::floor(amplitude/std::sqrt(1+tangent*tangent)));
        // Never enlarge the native normal: its fixed-point sphere radius is
        // compared directly against this dot product. Round the cone outward.
        c=std::min(c,int(std::floor(std::sqrt(amplitude*amplitude-double(s)*s))));
        out.planes[axis]=int16_t(s);out.planes[axis+1]=int16_t(c);
    }
    out.applied=true;return out;
}
}
