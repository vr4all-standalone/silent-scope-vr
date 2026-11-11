// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"
#include "scene_geometry.h"
namespace silent_scope::scene {
inline std::shared_ptr<const RawFrame> compose_frame(std::shared_ptr<const RawFrame> source,NativeLens lens,int width,int height,
                                                   std::vector<uint32_t> raster,std::vector<uint32_t> overlay){
    (void)raster;
    // Native geometry carries the exact camera used at its first draw. A later
    // screen update/menu toggle must not relabel an older Voodoo buffer.
    if(source&&source->camera_owned)lens=source->lens;
    if(!source||!source->complete||source->width!=uint32_t(width)||source->height!=uint32_t(height)||!lens.valid()||source->triangles.empty())return {};
    auto frame=std::make_shared<RawFrame>(*source);frame->lens=lens;
    static thread_local uint64_t sequence{};frame->sequence=++sequence;
    if(overlay.size()==size_t(width)*height){
        auto texture=std::make_shared<const TextureImage>(UINT64_MAX,frame->sequence,width,height,std::move(overlay));
        RawVertex v[4];const double xy[4][2]={{0,0},{double(width),0},{0,double(height)},{double(width),double(height)}};
        for(int i=0;i<4;++i){v[i].x=xy[i][0];v[i].y=xy[i][1];v[i].q=1/1.1;v[i].s[0]=v[i].x*v[i].q;v[i].t[0]=v[i].y*v[i].q;v[i].tq[0]=v[i].q;v[i].r=v[i].g=v[i].b=v[i].a=255;}
        for(auto ids:{std::array<int,3>{0,1,2},std::array<int,3>{2,1,3}}){
            RawTriangle t;t.overlay=true;t.texture[0]=texture;t.state.color_path=5;t.state.depth_mode=512;t.state.texture_mode=0x082410c5;
            t.state.alpha_mode=16|(1<<8)|(5<<12)|(4<<16)|(5<<20);for(int i=0;i<3;++i)t.vertex[i]=v[ids[i]];frame->triangles.push_back(std::move(t));
        }
    }
    return frame;
}
}
