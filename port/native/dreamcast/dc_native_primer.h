// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_scope_composite.h"
namespace silent_scope::dc_native_primer {
template<class Context> bool frame(std::string_view title,const Context& ctx){
    return title=="T9507N"&&!ctx.isRTT&&ctx.framebufferWidth==640&&ctx.framebufferHeight==480&&ctx.dcCamera.supported&&
        ctx.dcCamera.generation&&ctx.dcCamera.submit_sequence&&ctx.verts.size()<=65536&&ctx.idx.size()<=262144&&
        ctx.global_param_op.size()+ctx.global_param_pt.size()+ctx.global_param_tr.size()<=16384&&
        ctx.render_passes.size()<=64&&ctx.sortedTriangles.size()<=16384;
}
template<class Polygon> bool material(const Polygon& p,unsigned list){
    return list<=2&&p.pcw.full==0xa0800000&&p.isp.full==0x09000000&&p.tsp.full==(list==0?0x20800440u:0x00800440u)&&
        p.tcw.full==0&&p.tileclip==0x00100200;
}
namespace detail {
template<class Context> bool quad(const Context& ctx,uint32_t base){
    if(uint64_t(base)+4>ctx.verts.size())return false;
    // Native12cebc..cf98 copies the exact8c161e98 sprite template. UV is
    // uninitialized/NaN in decoded records because this native sprite is untextured.
    constexpr float xy[4][2]{{-1.5f,-.5f},{-.5f,-.5f},{-1.5f,-1.f},{-.5f,-1.f}};
    for(unsigned i=0;i<4;++i){const auto& v=ctx.verts[base+i];
        if(v.x!=xy[i][0]||v.y!=xy[i][1]||v.z!=.1f||v.col[0]||v.col[1]||v.col[2]||v.col[3])return false;
    }
    return true;
}
template<class Context,class Polygon> bool original_range(const Context& ctx,const Polygon& p,bool by_vertex,uint32_t base){
    if(by_vertex)return p.first==base&&p.count==4;
    if(p.first>ctx.idx.size()||p.count>ctx.idx.size()-p.first)return false;
    const uint64_t end=uint64_t(p.first)+p.count;
    for(uint64_t j=p.first;j+3<end;++j){
        // All four corners and both native triangles must belong to one whole
        // original sprite strip, not an arbitrary same-material vertex triple.
        if((j==p.first||ctx.idx[j-1]==UINT32_MAX)&&(j+4==end||ctx.idx[j+4]==UINT32_MAX)&&
           ctx.idx[j]==base&&ctx.idx[j+1]==base+1&&ctx.idx[j+2]==base+2&&ctx.idx[j+3]==base+3)return true;
    }
    return false;
}
}
template<class Context> bool proven_triangle(std::string_view title,const Context& ctx,unsigned list,std::size_t polygon,uint32_t a,uint32_t b,uint32_t c){
    if(!frame(title,ctx)||list>2||a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size()||a==b||a==c||b==c)return false;
    const auto& ps=list==0?ctx.global_param_op:list==1?ctx.global_param_pt:ctx.global_param_tr;
    if(polygon>=ps.size()||!material(ps[polygon],list))return false;
    const uint32_t least=std::min({a,b,c});
    for(unsigned shift=0;shift<2;++shift){if(least<shift)continue;const uint32_t base=least-shift;
        if(uint64_t(base)+3>=ctx.verts.size()||a<base||b<base||c<base||a>base+3||b>base+3||c>base+3)continue;
        const unsigned corners=(1u<<(a-base))|(1u<<(b-base))|(1u<<(c-base));
        if((corners!=7&&corners!=14)||!detail::quad(ctx,base))continue;
        // Autosorted batches may merge several original sprite material IDs.
        // Find the actual complete native primitive owning this vertex triple.
        for(std::size_t i=0;i<ps.size();++i){const auto& p=ps[i];if(!material(p,list)||!p.count)continue;
            const auto storage=list==2?dc_scope::translucent_storage(ctx,i):dc_scope::PolygonStorage::Indices;
            if(storage!=dc_scope::PolygonStorage::Unknown&&detail::original_range(ctx,p,storage==dc_scope::PolygonStorage::Vertices,base))return true;
        }
    }
    return false;
}
struct Range{uint32_t first{},count{3};bool flip{};};
// A rare same-material mixed batch is emitted one surviving native triangle at
// a time. All input indices are checked before any draw; Original/unknown frames
// return false and retain the caller's exact original draw. Restart parity is
// retained. No whole-material or whole-batch suppression is possible here.
template<class Context,class Emit> bool draw_filtered(std::string_view title,const Context& ctx,unsigned list,std::size_t polygon,
        uint32_t first,uint32_t count,bool triangles,bool vr_world,Emit emit){
    if(!vr_world||!frame(title,ctx)||list>2||first>ctx.idx.size()||count>ctx.idx.size()-first||(triangles&&count%3))return false;
    const auto& ps=list==0?ctx.global_param_op:list==1?ctx.global_param_pt:ctx.global_param_tr;
    if(polygon>=ps.size()||!material(ps[polygon],list))return false;
    const uint64_t end=uint64_t(first)+count;
    for(uint64_t j=first;j<end;++j)if(ctx.idx[j]>=ctx.verts.size()&&(triangles||ctx.idx[j]!=UINT32_MAX))return false;
    const auto walk=[&](auto visit){uint32_t segment=first;
        for(uint64_t j=first;j<end;){
            if(!triangles&&ctx.idx[j]==UINT32_MAX){segment=uint32_t(j+1);++j;continue;}
            if(!triangles&&j<uint64_t(segment)+2){++j;continue;}
            const uint32_t start=uint32_t(triangles?j:j-2);
            visit(start,!triangles&&((start-segment)&1)!=0,proven_triangle(title,ctx,list,polygon,ctx.idx[start],ctx.idx[start+1],ctx.idx[start+2]));
            j+=triangles?3:1;
        }
    };
    bool any=false;walk([&](uint32_t,bool,bool skip){any=any||skip;});if(!any)return false;
    walk([&](uint32_t start,bool flip,bool skip){if(!skip)emit(Range{start,3,flip});});return true;
}
}
