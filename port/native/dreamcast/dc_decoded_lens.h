// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_lens_ownership.h"
#include "dc_native_primer.h"
namespace silent_scope { namespace dreamcast {
struct DecodedLensResult {
    SubmittedLens lens;uint64_t world_triangles{},ui_triangles{},unknown_triangles{},mixed_triangles{};
    bool malformed{},unsupported_modifiers{};
    bool has_problem{};unsigned problem_list{};size_t problem_polygon{};std::array<uint32_t,3> problem_vertices{};
    uint64_t skipped_never_polygons{},skipped_primers{};
};
struct IgnoreLensProblem {
    void operator()(unsigned, size_t, uint32_t, uint32_t, uint32_t) const noexcept {}
};
// Render-thread read-only finalization over immutable TA packet provenance.
// The decoded UI mask must prove each exception, including the complete native
// scope group. All other finite native triangles, including constant-q world
// planes, must have coherent source ownership. No current SDK or setting reads.
template<class Context,class UiMask,class OnProblem=IgnoreLensProblem>
DecodedLensResult finalize_decoded_lens(const Context& ctx,const UiMask& ui,OnProblem on_problem={}){
    DecodedLensResult out;
    if(ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||!ctx.dcCamera.supported
        ||!ctx.dcCamera.generation||!ctx.dcCamera.submit_sequence)return out;
    if(ctx.verts.size()>65536||ctx.idx.size()>262144||ctx.dcVertexLens.size()!=ctx.verts.size()){out.malformed=true;return out;}
    if(!ctx.modtrig.empty()){out.unsupported_modifiers=true;return out;}
    SubmittedLens owned;
    auto problem=[&](unsigned list,size_t polygon,uint32_t a,uint32_t b,uint32_t c){
        if(!out.has_problem){
            out.has_problem=true;
            out.problem_list=list;
            out.problem_polygon=polygon;
            out.problem_vertices={a,b,c};
        }
        on_problem(list,polygon,a,b,c);
    };
    auto triangle=[&](unsigned list,size_t polygon,uint32_t a,uint32_t b,uint32_t c){
        if(a==UINT32_MAX||b==UINT32_MAX||c==UINT32_MAX||a==b||a==c||b==c)return;
        if(a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size()){out.malformed=true;return;}
        const auto& va=ctx.verts[a];const auto& vb=ctx.verts[b];const auto& vc=ctx.verts[c];
        for(const auto* v:{&va,&vb,&vc})if(!std::isfinite(v->x)||!std::isfinite(v->y)||!std::isfinite(v->z)||v->z<=0)return;
        const double area=double(vb.x-va.x)*(vc.y-va.y)-double(vb.y-va.y)*(vc.x-va.x);
        if(!std::isfinite(area)||std::abs(area)<=1e-6)return;
        if(dc_native_primer::proven_triangle("T9507N",ctx,list,polygon,a,b,c)){++out.skipped_primers;return;}
        if(ui.proven_ui_triangle(ctx,list,polygon,a,b,c)){++out.ui_triangles;return;}
        const auto& la=ctx.dcVertexLens[a];const auto& lb=ctx.dcVertexLens[b];const auto& lc=ctx.dcVertexLens[c];
        if(!complete_main_lens(la)||!complete_main_lens(lb)||!complete_main_lens(lc)
            ||la.generation!=ctx.dcCamera.generation||lb.generation!=ctx.dcCamera.generation||lc.generation!=ctx.dcCamera.generation){++out.unknown_triangles;problem(list,polygon,a,b,c);return;}
        if(!same_projection(la,lb)||!same_projection(la,lc)||(owned.valid&&!same_projection(owned,la))){++out.mixed_triangles;problem(list,polygon,a,b,c);return;}
        owned=la;++out.world_triangles;
    };
    auto strips=[&](unsigned list,const auto& polygons,size_t begin,size_t end,bool autosort=false){
        if(begin>end||end>polygons.size()){out.malformed=true;return;}
        for(size_t n=begin;n<end;++n){const auto& p=polygons[n];
            // Match GLES DrawList exactly: PT and autosorted TR override the
            // native depth function, so Never only skips OP/unsorted TR.
            if((list==0||(list==2&&!autosort))&&((p.isp.full>>29)&7)==0){++out.skipped_never_polygons;continue;}
            if(p.first>ctx.idx.size()||p.count>ctx.idx.size()-p.first){out.malformed=true;continue;}
            for(size_t j=size_t(p.first)+2;j<size_t(p.first)+p.count;++j)triangle(list,n,ctx.idx[j-2],ctx.idx[j-1],ctx.idx[j]);
        }
    };
    // OP0 is the engine-generated background plane, not a native vertex packet.
    strips(0,ctx.global_param_op,ctx.global_param_op.empty()?0:1,ctx.global_param_op.size());
    strips(1,ctx.global_param_pt,0,ctx.global_param_pt.size());
    uint64_t previous_tr=0,previous_sorted=0;
    for(const auto& pass:ctx.render_passes){
        if(pass.tr_count<previous_tr||pass.tr_count>ctx.global_param_tr.size()
            ||pass.sorted_tr_count<previous_sorted||pass.sorted_tr_count>ctx.sortedTriangles.size()){out.malformed=true;break;}
        if(pass.sorted_tr_count>previous_sorted){
            if(!pass.autosort){out.malformed=true;break;}
            for(uint64_t i=previous_sorted;i<pass.sorted_tr_count;++i){const auto& batch=ctx.sortedTriangles[size_t(i)];
                if(batch.polyIndex>=ctx.global_param_tr.size()||batch.first>ctx.idx.size()||batch.count>ctx.idx.size()-batch.first||batch.count%3){out.malformed=true;continue;}
                for(size_t j=batch.first;j<size_t(batch.first)+batch.count;j+=3)triangle(2,batch.polyIndex,ctx.idx[j],ctx.idx[j+1],ctx.idx[j+2]);
            }
        }else strips(2,ctx.global_param_tr,size_t(previous_tr),pass.tr_count,pass.autosort);
        previous_tr=pass.tr_count;previous_sorted=pass.sorted_tr_count;
    }
    if(previous_tr!=ctx.global_param_tr.size()||previous_sorted!=ctx.sortedTriangles.size())out.malformed=true;
    if(!out.malformed&&!out.unknown_triangles&&!out.mixed_triangles&&out.world_triangles>=4)
        out.lens=publish_owned_lens(owned,ctx.dcCamera.generation,ctx.dcCamera.submit_sequence,640,480,false,true);
    return out;
}
}}
