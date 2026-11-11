// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_hud.h"
#include "dc_scope_composite.h"
#include "dc_hud_message.h"
namespace silent_scope::dc_scope_hud {
inline dc_scope::GeometryGroup& draw_geometry(){static thread_local dc_scope::GeometryGroup value;return value;}
inline dc_hud_message::HurryUpGroup& draw_message(){static thread_local dc_hud_message::HurryUpGroup value;return value;}
class ScopedDraw {
    dc_scope::GeometryGroup previous_;
    dc_hud_message::HurryUpGroup previous_message_;
public:
    template<class Context> ScopedDraw(std::string_view product,const Context& ctx,bool vr_world):previous_(draw_geometry()),previous_message_(draw_message()){
        if(vr_world)draw_message().configure(product,ctx);else draw_message()={};
        auto& geometry=draw_geometry();
        if(!vr_world||product!="T9507N")geometry.clear();
        else if(dc_scope::composite_draw_mask().geometry_proof().matches_frame(ctx))
            geometry=dc_scope::composite_draw_mask().geometry_proof(); // Reuse already verified current proof.
        else geometry.configure(product,ctx);
    }
    ~ScopedDraw(){draw_geometry()=previous_;draw_message()=previous_message_;}
    ScopedDraw(const ScopedDraw&)=delete;ScopedDraw& operator=(const ScopedDraw&)=delete;
};
template<class Context,class Polygon> dc_hud::Decision classify(const dc_hud::Gate& gate,const Context& ctx,const Polygon& p,
        std::size_t polygon,uint32_t a,uint32_t b,uint32_t c)noexcept{
    if(!dc_hud::enabled(gate)||a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size())return {};
    const auto hud=dc_hud::classify_detailed(gate,p,ctx.verts[a],ctx.verts[b],ctx.verts[c]);
    if(hud.layer!=dc_hud::Layer::World)return hud;
    const auto message=draw_message().classify(ctx,2,polygon,a,b,c);
    if(message.layer!=dc_hud::Layer::World)return message;
    const auto& geometry=draw_geometry();
    if(geometry.contains_triangle(ctx,polygon,a,b,c))return {dc_hud::Layer::ScopeFallback,geometry.native_scale()};
    return {};
}
template<class Context,class Polygon,class Emit> bool draw_ranges(const dc_hud::Gate& gate,const Context& ctx,const Polygon& p,
        std::size_t polygon,uint32_t first,uint32_t count,dc_hud::Primitive mode,Emit emit){
    return dc_hud::draw_ranges_with(gate,ctx,p,first,count,mode,[&](uint32_t a,uint32_t b,uint32_t c){
        return classify(gate,ctx,p,polygon,a,b,c);
    },emit);
}
}
