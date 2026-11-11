// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_hud.h"
#include "dc_scope_composite.h"
#include "dc_hud_message.h"
namespace silent_scope::dc_ta_ui {
class DecodedUiMask {
    const void* frame_{};uint64_t generation_{},submission_{};bool eligible_{};
    dc_scope::GeometryGroup scope_;
    dc_hud_message::HurryUpGroup message_;
public:
    // Finalize after native TA parsing, with the actual immutable submission
    // stamp. This never reads RAM/GL or turns unknown constant-q into UI.
    template<class Context> bool configure(std::string_view title,const Context& ctx)noexcept{
        *this=DecodedUiMask{};
        if(title!="T9507N"||ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||
           !ctx.dcCamera.supported||!ctx.dcCamera.generation||!ctx.dcCamera.submit_sequence||
           ctx.verts.size()>65536||ctx.idx.size()>262144||ctx.render_passes.size()>64||ctx.sortedTriangles.size()>16384||
           ctx.global_param_op.size()+ctx.global_param_pt.size()+ctx.global_param_tr.size()>16384)return false;
        frame_=&ctx;generation_=ctx.dcCamera.generation;submission_=ctx.dcCamera.submit_sequence;eligible_=true;
        scope_.configure(title,ctx); // No group means no scope exemptions; HUD may still be known.
        message_.configure(title,ctx);
        return true;
    }
    template<class Context> bool proven_ui_triangle(const Context& ctx,unsigned list,std::size_t polygon,uint32_t a,uint32_t b,uint32_t c)const noexcept{
        if(!eligible_||frame_!=&ctx||generation_!=ctx.dcCamera.generation||submission_!=ctx.dcCamera.submit_sequence||
           ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||!ctx.dcCamera.supported||
           list!=2||polygon>=ctx.global_param_tr.size()||a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size())return false;
        const auto& p=ctx.global_param_tr[polygon];
        const auto gate=dc_hud::frame_gate("T9507N",ctx,true,list);
        if(dc_hud::classify_detailed(gate,p,ctx.verts[a],ctx.verts[b],ctx.verts[c]).layer!=dc_hud::Layer::World)return true;
        if(message_.classify(ctx,list,polygon,a,b,c).layer!=dc_hud::Layer::World)return true;
        // A merged material ID is insufficient: the actual vertex triple must
        // be an original triangle from the complete verified fan/rim group.
        return scope_.contains_triangle(ctx,polygon,a,b,c);
    }
};
}
