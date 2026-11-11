// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// T9507N only. Capture on the SH4 thread at STARTRENDER. This host-side value
// travels with its TA submission; consumers never read live emulated memory.
// Native widening is separately authorized/proved. This module never writes RAM.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>
namespace silent_scope { namespace dreamcast {
constexpr uint32_t LensSdkBase=0x8cd1f9c0,LensCacheBase=0x8cd206d4;
constexpr uint32_t LensSourceMain=0x8c1b0108,LensMode=0x8c1b0114;
// Exact US executable: cache refresh -> chunk projector and per-object culler.
// Source-derived production guards, not signatures selected from a RAM scan.
constexpr std::array<std::pair<uint32_t,uint32_t>,12> LensCodeGuard{{
    {0x8c111cf4,0xe004d59bu},{0x8c111d08,0xf258f338u},
    {0x8c111d14,0xf232f328u},{0x8c111d20,0xe010f437u},
    {0x8c111d28,0xf437000bu},{0x8c14066c,0xf149f049u},
    {0x8c140670,0xf349f249u},{0x8c124f32,0xf228f118u},
    {0x8c0fc4b0,0xf3382212u},{0x8c0fc4b4,0xd20df2a8u},
    {0x8c0fc4b8,0xfc2af232u},{0x8c0fc4bc,0xf2a8f328u}}};
struct LensRequest {
    bool enabled{},wide_proven{},wider{};
    uint64_t revision{};
};
// Required evidence from native projection/buffer construction, tied to the
// submitted TA identity. Live SDK/cache at STARTRENDER is insufficient: the
// controlled45/55 transition capture proved it can lead geometry by one frame.
// No production receipt publisher is enabled until that ownership is proved.
struct LensOwnership {
    bool verified{};
    uint64_t generation{},submit_sequence{};
    float fx{},fy{},cx{},cy{};
};
struct SubmittedLens {
    uint64_t generation{},submit_sequence{},request_revision{};
    bool supported{},valid{};
    float fx{},fy{},cx{},cy{},width{},height{},factor{};
    float stock_fx{555.f},stock_fy{555.f};
};
inline float lens_float(uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
template<class Read> SubmittedLens capture_lens(std::string_view title,bool rtt,
        unsigned width,unsigned height,uint64_t generation,uint64_t sequence,
        const LensRequest& request,const LensOwnership& ownership,Read read){
    SubmittedLens lens;
    if(title!="T9507N")return lens;
    lens.supported=true;lens.generation=generation;lens.submit_sequence=sequence;
    lens.request_revision=request.revision;
    if(!request.enabled||rtt||width!=640||height!=480||!generation||!sequence)return lens;
    if(!ownership.verified||ownership.generation!=generation||ownership.submit_sequence!=sequence)return lens;
    for(auto guard:LensCodeGuard)if(read(guard.first)!=guard.second)return lens;
    // Check the native source rather than confusing a modified draw lens with
    // the input/hit camera. Scope source 8c1b010c is deliberately not consulted.
    if(read(LensSourceMain)!=0x440ac000u||read(LensMode)!=0)return lens;
    const uint32_t focal=read(LensSdkBase);
    if(focal!=0x440ac000u&&!(request.wide_proven&&focal==0x43390000u))return lens;
    if(read(LensSdkBase+4)!=0x44200000u||read(LensSdkBase+8)!=0x43f00000u
        ||read(LensSdkBase+12)!=0x43a00000u||read(LensSdkBase+16)!=0x43700000u
        ||read(LensSdkBase+52)!=0x3f800000u||read(LensSdkBase+56)!=0x3f800000u
        ||read(LensCacheBase)!=focal||read(LensCacheBase+4)!=focal
        ||read(LensCacheBase+8)!=0x43a00000u||read(LensCacheBase+12)!=0x43700000u)return lens;
    const float actual=lens_float(focal);
    if(ownership.fx!=actual||ownership.fy!=actual||ownership.cx!=320||ownership.cy!=240)return lens;
    lens.fx=lens.fy=actual;lens.cx=320;lens.cy=240;
    lens.width=640;lens.height=480;lens.factor=focal==0x440ac000u?1.f:3.f;
    lens.valid=true;return lens;
}
enum class LensStatus {Unavailable,Pending,Original,Wider};
inline LensStatus lens_status(const SubmittedLens& lens,const LensRequest& request,uint64_t current_generation){
    if(!request.enabled||!lens.supported||!lens.valid||!current_generation||lens.generation!=current_generation)
        return LensStatus::Unavailable;
    if(lens.request_revision!=request.revision||lens.factor!=(request.wider?3.f:1.f))return LensStatus::Pending;
    return lens.factor==3.f?LensStatus::Wider:LensStatus::Original;
}
struct EyePoint {float x{},y{},z{};};
struct PixelPoint {float x{},y{};};
inline bool unproject(const SubmittedLens& lens,float x,float y,float q,EyePoint& out){
    if(!lens.valid||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(q)||q<=0)return false;
    const float w=1.f/q;const EyePoint p{(x-lens.cx)*w/lens.fx,(y-lens.cy)*w/lens.fy,-w};
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return false;
    out=p;return true;
}
inline bool project_lens(const SubmittedLens& lens,const EyePoint& p,float fx,float fy,PixelPoint& out){
    if(!lens.valid||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.z>=0)return false;
    const PixelPoint pixel{lens.cx+p.x*fx/-p.z,lens.cy+p.y*fy/-p.z};
    if(!std::isfinite(pixel.x)||!std::isfinite(pixel.y))return false;
    out=pixel;return true;
}
inline bool project_draw(const SubmittedLens& lens,const EyePoint& p,PixelPoint& out){return project_lens(lens,p,lens.fx,lens.fy,out);}
// Stock projection is a geometric reference. It does not assert this console
// game's native mouse endpoint is a pixel gun; the angular adapter owns input.
inline bool project_stock(const SubmittedLens& lens,const EyePoint& p,PixelPoint& out){return project_lens(lens,p,lens.stock_fx,lens.stock_fy,out);}
inline bool triangle_intersects_lens_raster(PixelPoint a,PixelPoint b,PixelPoint c,float width,float height){
    std::array<PixelPoint,12> polygon{{a,b,c}},next{};unsigned count=3;
    for(unsigned edge=0;edge<4;++edge){
        unsigned size=0;
        auto distance=[&](PixelPoint p){return edge==0?p.x:edge==1?width-p.x:edge==2?p.y:height-p.y;};
        PixelPoint previous=polygon[count-1];float da=distance(previous);
        for(unsigned i=0;i<count;++i){const PixelPoint current=polygon[i];const float db=distance(current);
            if((da>=0)!=(db>=0)){
                const float t=da/(da-db);
                next[size++]={previous.x+t*(current.x-previous.x),previous.y+t*(current.y-previous.y)};
            }
            if(db>=0)next[size++]=current;
            previous=current;da=db;
        }
        if(size<3)return false;
        polygon=next;count=size;
    }
    double area=0;for(unsigned i=0;i<count;++i){const auto p=polygon[i],q=polygon[(i+1)%count];area+=double(p.x)*q.y-double(p.y)*q.x;}
    return std::isfinite(area)&&std::abs(area)>1e-6;
}
// Conservative render domain, distinct from lens validity and native game state.
// OP/PT are native index strips. Excluding all TR avoids assuming the storage
// mode of sorted TR and excludes the measured HUD, scope feed and scope rim.
// Four sloped, non-near surfaces are required: layered fixed-depth title/UI
// planes cannot establish a world merely by inheriting a valid555 camera.
template<class Context> bool has_world_geometry(const Context& ctx){
    const auto& lens=ctx.dcLens;
    if(ctx.isRTT||!lens.supported||!lens.valid||!lens.generation||!lens.submit_sequence
        ||lens.generation!=ctx.dcCamera.generation||lens.submit_sequence!=ctx.dcCamera.submit_sequence)return false;
    unsigned surfaces=0;
    auto strips=[&](const auto& polygons,bool opaque){
        for(size_t n=opaque?1:0;n<polygons.size();++n){
            const auto& polygon=polygons[n];
            // The measured native feed/rim are TR; also reject these exact
            // materials if encountered in another list. Address alone never
            // positively identifies a world surface.
            if(polygon.tcw.full==0x0c0a5800u||polygon.tcw.full==0x5004c800u)continue;
            if(polygon.first>ctx.idx.size()||polygon.count>ctx.idx.size()-polygon.first)continue;
            const size_t end=size_t(polygon.first)+polygon.count;
            for(size_t i=size_t(polygon.first)+2;i<end;++i){
                const uint32_t a=ctx.idx[i-2],b=ctx.idx[i-1],c=ctx.idx[i];
                if(a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size()||a==b||a==c||b==c)continue;
                const auto& va=ctx.verts[a];const auto& vb=ctx.verts[b];const auto& vc=ctx.verts[c];
                bool finite=true;for(const auto* v:{&va,&vb,&vc})
                    if(!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||v->z<=0||v->z>.25f)finite=false;
                if(!finite)continue;
                const float low=std::min({va.z,vb.z,vc.z}),high=std::max({va.z,vb.z,vc.z});
                if(high-low<high*.01f)continue;
                if(std::max({va.x,vb.x,vc.x})<=0||std::min({va.x,vb.x,vc.x})>=lens.width
                    ||std::max({va.y,vb.y,vc.y})<=0||std::min({va.y,vb.y,vc.y})>=lens.height)continue;
                const double area=double(vb.x-va.x)*(vc.y-va.y)-double(vb.y-va.y)*(vc.x-va.x);
                if(std::isfinite(area)&&std::abs(area)>=2
                    &&triangle_intersects_lens_raster({va.x,va.y},{vb.x,vb.y},{vc.x,vc.y},lens.width,lens.height)
                    &&++surfaces>=4)return true;
            }
        }
        return false;
    };
    return strips(ctx.global_param_op,true)||strips(ctx.global_param_pt,false);
}
}}
