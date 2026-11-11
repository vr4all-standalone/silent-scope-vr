// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string_view>
#include <algorithm>
#include <cmath>
#include "dc_native_menu.h"
namespace silent_scope::dc_ui {
enum class Domain {Unknown,NativeUi,World};
// No RAM reads or previous-frame latch. Executable/device verification remains
// independently mandatory in the native mouse consumer before emitting Left.
template<class Context> Domain classify(std::string_view title,const Context* frame,bool owned_world){
    if(!frame||title!="T9507N")return Domain::Unknown;
    const auto& c=*frame;
    if(c.isRTT||c.framebufferWidth!=640||c.framebufferHeight!=480||!c.dcCamera.supported||
       !c.dcCamera.generation||!c.dcCamera.submit_sequence)return Domain::Unknown;
    const auto& lens=c.dcLens;
    if(owned_world&&lens.supported&&lens.valid&&lens.generation==c.dcCamera.generation&&
       lens.submit_sequence==c.dcCamera.submit_sequence)return Domain::World;
    dc_native_menu::MainMenuProof main_menu;
    if(main_menu.configure(title,c))return Domain::NativeUi;
    if(c.verts.size()>65536||c.idx.size()>262144||c.render_passes.empty()||c.render_passes.size()>64||
       c.global_param_op.size()+c.global_param_pt.size()+c.global_param_tr.size()>16384||c.sortedTriangles.size()>16384)return Domain::Unknown;
    const auto no_scope=[](const auto& list){
        for(const auto& p:list)if(p.count&&(p.pcw.full&8)){
            const auto address=(p.tcw.full&0x1fffff)*8;
            // Negative evidence only: no guessed address-based removal or UI label.
            if(address==0x52c000||address==0x264000)return false;
        }
        return true;
    };
    if(!no_scope(c.global_param_op)||!no_scope(c.global_param_pt)||!no_scope(c.global_param_tr))return Domain::Unknown;
    // Measured native boot/title background. PVR overscans this untextured
    // black quad horizontally; it is not foreground UI and cannot alone grant
    // clicks. Only this exact tuple bypasses the foreground raster bounds.
    const auto captured_background=[&](const auto& p){
        if(p.first!=0||p.count!=4||p.isp.full!=0xe0800000||p.tsp.full!=0x208004c0||
           p.tcw.full!=0||p.pcw.full!=2||p.tileclip!=0||c.idx.size()<4||c.verts.size()<4)return false;
        for(unsigned i=0;i<4;++i){
            if(c.idx[i]!=i)return false;
            const auto& v=c.verts[i];
            if(v.x!=(i&1?896.f:-256.f)||v.y!=(i&2?480.f:0.f)||v.z!=8.999988494906574e-6f||v.u!=0||v.v!=0||
               v.col[0]!=0||v.col[1]!=0||v.col[2]!=0||v.col[3]!=255)return false;
        }
        return true;
    };
    unsigned foreground=0;
    const auto triangle=[&](uint32_t a,uint32_t b,uint32_t d,bool background){
        if(a>=c.verts.size()||b>=c.verts.size()||d>=c.verts.size())return false;
        const auto& va=c.verts[a];const auto& vb=c.verts[b];const auto& vd=c.verts[d];
        for(const auto* v:{&va,&vb,&vd})if(!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||
            v->z<=0||v->x< -1||v->x>641||v->y< -1||v->y>481)return false;
        const double area=double(vb.x-va.x)*(vd.y-va.y)-double(vb.y-va.y)*(vd.x-va.x);
        if(!std::isfinite(area))return false;
        if(a==b||a==d||b==d||std::abs(area)<1e-6)return true; // Native strip connectors.
        const float low=std::min({va.z,vb.z,vd.z}),high=std::max({va.z,vb.z,vd.z});
        if(high-low>high*1e-5f)return false;
        if(!background&&std::max({va.x,vb.x,vd.x})>0&&std::min({va.x,vb.x,vd.x})<640&&
           std::max({va.y,vb.y,vd.y})>0&&std::min({va.y,vb.y,vd.y})<480)++foreground;
        return true;
    };
    const auto strip=[&](const auto& p,bool background){
        if(!p.count)return true;
        if(background&&captured_background(p))return true;
        if(p.count<3||p.first>c.idx.size()||p.count>c.idx.size()-p.first)return false;
        uint64_t segment=p.first;
        for(uint64_t i=p.first;i<uint64_t(p.first)+p.count;++i){
            const auto index=c.idx[i];if(index==UINT32_MAX){segment=i+1;continue;}
            if(index>=c.verts.size())return false;
            if(i>=segment+2&&!triangle(c.idx[i-2],c.idx[i-1],index,background))return false;
        }return true;
    };
    uint32_t previous_op=0,previous_pt=0,previous_tr=0,previous_sorted=0;
    for(const auto& pass:c.render_passes){
        if(pass.op_count<previous_op||pass.op_count>c.global_param_op.size()||pass.pt_count<previous_pt||pass.pt_count>c.global_param_pt.size()||
           pass.tr_count<previous_tr||pass.tr_count>c.global_param_tr.size()||pass.sorted_tr_count<previous_sorted||pass.sorted_tr_count>c.sortedTriangles.size())return Domain::Unknown;
        for(uint32_t i=previous_op;i<pass.op_count;++i)if(!strip(c.global_param_op[i],i==0))return Domain::Unknown;
        for(uint32_t i=previous_pt;i<pass.pt_count;++i)if(!strip(c.global_param_pt[i],false))return Domain::Unknown;
        if(pass.sorted_tr_count>previous_sorted){
            if(!pass.autosort)return Domain::Unknown;
            for(uint32_t i=previous_sorted;i<pass.sorted_tr_count;++i){const auto& p=c.sortedTriangles[i];
                if(p.polyIndex<previous_tr||p.polyIndex>=pass.tr_count||p.first>c.idx.size()||p.count>c.idx.size()-p.first||p.count%3)return Domain::Unknown;
                for(uint64_t j=p.first;j<uint64_t(p.first)+p.count;j+=3)if(!triangle(c.idx[j],c.idx[j+1],c.idx[j+2],false))return Domain::Unknown;
            }
        }else for(uint32_t i=previous_tr;i<pass.tr_count;++i)if(!strip(c.global_param_tr[i],false))return Domain::Unknown;
        previous_op=pass.op_count;previous_pt=pass.pt_count;previous_tr=pass.tr_count;previous_sorted=pass.sorted_tr_count;
    }
    if(previous_op!=c.global_param_op.size()||previous_pt!=c.global_param_pt.size()||previous_tr!=c.global_param_tr.size()||
       previous_sorted!=c.sortedTriangles.size()||foreground<2)return Domain::Unknown;
    return Domain::NativeUi;
}
}
