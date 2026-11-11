// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"
#include <algorithm>
namespace silent_scope::scene {
// Preserve native visibility at every captured background pixel, including thin
// foreground edges that fall between mesh samples. Unknown depth (65535) is
// left alone; zero is a valid saturated-near occluder.
inline bool native_background_depth_supported(const RawTriangle& t) {
    return (t.state.depth_mode&24)==24&&!(t.state.depth_mode&(1u<<21));
}
inline uint32_t encode_background_w(double q) {
    if(q>=1)return 0;
    if(!(q>=1./65536))return 65535;
    const int exponent=-int(std::floor(std::log2(q)))-1;
    return ((exponent<<12)|(uint32_t(std::ldexp(q,13+exponent))^8191))+1;
}
inline bool native_depth_passes(int source,int dest,unsigned compare) {
    switch(compare){case 0:return false;case 1:return source<dest;case 2:return source==dest;
        case 3:return source<=dest;case 4:return source>dest;case 5:return source!=dest;
        case 6:return source>=dest;default:return true;}
}
inline bool background_depth_rejects(const RawFrame& f,const RawTriangle& t,Vec3 point,double native_q) {
    if(!native_background_depth_supported(t)||!f.lens.valid()||f.background_depth.size()!=size_t(f.width)*f.height)return false;
    auto uv=f.lens.project_point(point);if(!uv.valid)return false;
    const int x=int(std::floor(uv.u*(f.lens.width-1))),y=int(std::floor(uv.v*(f.lens.height-1)));
    if(x<0||y<0||x>=int(f.width)||y>=int(f.height))return false;
    const auto dest=f.background_depth[size_t(y)*f.width+x];if(dest==65535)return false;
    const auto mode=t.state.depth_mode;
    int source=encode_background_w(native_q);
    if(mode&(1u<<16))source=std::clamp(source+int16_t(t.depth_constant),0,65535);
    if(mode&(1u<<20))source=uint16_t(t.depth_constant);
    return !native_depth_passes(source,dest,(mode>>5)&7);
}
}

