// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_scope.h"
#include <array>
#include <cstddef>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <limits>

namespace silent_scope::dc_scope {
enum class PolygonStorage { Unknown, Vertices, Indices };
template<class Context> PolygonStorage translucent_storage(const Context& ctx,std::size_t index) {
    uint64_t previous_tr=0,previous_sorted=0;
    for(const auto& pass:ctx.render_passes){
        if(pass.tr_count<previous_tr||pass.tr_count>ctx.global_param_tr.size()||
           pass.sorted_tr_count<previous_sorted||pass.sorted_tr_count>ctx.sortedTriangles.size())return PolygonStorage::Unknown;
        if(index>=previous_tr&&index<pass.tr_count){
            if(pass.sorted_tr_count>previous_sorted)return pass.autosort?PolygonStorage::Vertices:PolygonStorage::Unknown;
            return PolygonStorage::Indices;
        }
        previous_tr=pass.tr_count;previous_sorted=pass.sorted_tr_count;
    }
    return PolygonStorage::Unknown;
}
namespace composite_detail {
struct Point {double x{},y{},q{},u{},v{};std::array<unsigned,4> color{};};
template<class Context,class Polygon> bool point(const Context& ctx,const Polygon& p,PolygonStorage storage,unsigned offset,Point& out){
    const uint64_t at=uint64_t(p.first)+offset;uint64_t index=at;
    if(storage==PolygonStorage::Indices){if(at>=ctx.idx.size())return false;index=ctx.idx[at];}
    else if(storage!=PolygonStorage::Vertices)return false;
    if(index>=ctx.verts.size())return false;
    const auto& v=ctx.verts[index];out={v.x,v.y,v.z,v.u,v.v,{unsigned(v.col[0]),unsigned(v.col[1]),unsigned(v.col[2]),unsigned(v.col[3])}};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.q)&&std::isfinite(out.u)&&std::isfinite(out.v);
}
template<class P> unsigned address(const P& p){return(p.tcw.full&0x1fffffu)*8;}
template<class P> int material(const P& p){
    if(p.isp.full!=0xc8800000u||p.pcw.full!=0x828c002au||p.tileclip!=0x100200u)return 0;
    if(p.tcw.full==0x0c0a5800u&&p.tsp.full==0x9491a4edu)return 1;
    if(p.tcw.full==0x5004c800u&&p.tsp.full==0x9491a4dbu)return 2;
    return 0;
}
inline bool center_uv(const Point& p){return p.u==.5&&p.v==127./256;}
}
// Pure immutable geometry proof; no GPU handle, time, current GL context or
// rifle authorization. Every consumer uses this same complete26 validator.
class GeometryGroup {
public:
    template<class Context> bool configure(std::string_view product,const Context& ctx)noexcept{
        clear();
        if(product!="T9507N"||ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||
           !ctx.dcCamera.generation||!ctx.dcCamera.submit_sequence||ctx.global_param_tr.size()>16384||
           ctx.verts.size()>65536||ctx.idx.size()>262144)return false;
        uint64_t previous_tr=0,previous_sorted=0;
        for(const auto& pass:ctx.render_passes){
            if(pass.tr_count<previous_tr||pass.tr_count>ctx.global_param_tr.size()||
               pass.sorted_tr_count<previous_sorted||pass.sorted_tr_count>ctx.sortedTriangles.size()||
               (pass.sorted_tr_count>previous_sorted&&!pass.autosort))return false;
            previous_tr=pass.tr_count;previous_sorted=pass.sorted_tr_count;
        }
        if(previous_tr!=ctx.global_param_tr.size()||previous_sorted!=ctx.sortedTriangles.size())return false;
        const auto relevant=[](const auto& p){const auto a=composite_detail::address(p);return a==0x52c000u||a==0x264000u;};
        // A newly encountered use of these addresses cannot be guessed away.
        for(const auto& p:ctx.global_param_op)if(p.count&&relevant(p))return false;
        for(const auto& p:ctx.global_param_pt)if(p.count&&relevant(p))return false;
        unsigned total=0,feeds=0,rims=0;double cx=0,cy=0;bool center_found=false;
        std::array<PolygonStorage,26> storage{};
        for(std::size_t i=0;i<ctx.global_param_tr.size();++i){const auto& p=ctx.global_param_tr[i];if(!relevant(p))continue;
            if(total==26)return false;
            const int kind=composite_detail::material(p);if(!kind)return false;
            const auto mode=translucent_storage(ctx,i);if(mode==PolygonStorage::Unknown)return false;
            if(kind==1){
                if(p.count!=3&&p.count!=5)return false;
                unsigned centers=0;
                for(unsigned j=0;j<p.count;++j){composite_detail::Point v;if(!composite_detail::point(ctx,p,mode,j,v))return false;
                    if(composite_detail::center_uv(v)){
                        if(center_found&&(std::abs(cx-v.x)>.001||std::abs(cy-v.y)>.001))return false;
                        cx=v.x;cy=v.y;center_found=true;++centers;
                    }
                }
                if(centers!=1)return false;
                ++feeds;
            }else{if(p.count<8||p.count>10)return false;++rims;}
            storage[total]=mode;indices_[total++]=i;
        }
        if(total!=26||feeds!=22||rims!=4||!center_found||cx<0||cx>640||cy<0||cy>480)return false;
        // This title's projected UI can use555 or185 independently of the main
        // world's lens at transitions. Infer only the two measured whole-group
        // scales, then require every fan/rim vertex to agree with that tuple.
        double feed_x=0,feed_y=0;
        for(unsigned k=0;k<26;++k){const auto& p=ctx.global_param_tr[indices_[k]];
            if(composite_detail::material(p)!=1)continue;
            for(unsigned j=0;j<p.count;++j){composite_detail::Point v;
                if(!composite_detail::point(ctx,p,storage[k],j,v))return false;
                feed_x=std::max(feed_x,std::abs(v.x-cx));feed_y=std::max(feed_y,std::abs(v.y-cy));}}
        double native_scale=0;
        for(double candidate:{1.,3.})if(std::abs(feed_x*candidate-120.5890197753906)<.003&&
                std::abs(feed_y*candidate-112.6999816894531)<.003)native_scale=candidate;
        if(native_scale==0)return false;
        std::array<bool,64> edges{};unsigned triangles=0,rim8=0,rim9=0,rim10=0;std::array<bool,4> quadrants{};
        for(unsigned k=0;k<26;++k){const auto& p=ctx.global_param_tr[indices_[k]];const int kind=composite_detail::material(p);
            std::array<composite_detail::Point,10> vertices{};
            double lo_x=std::numeric_limits<double>::infinity(),lo_y=lo_x,hi_x=-lo_x,hi_y=-lo_x,sum_x=0,sum_y=0;
            const std::array<unsigned,4> color=kind==1?std::array<unsigned,4>{254,254,254,255}:std::array<unsigned,4>{176,176,176,255};
            for(unsigned j=0;j<p.count;++j){auto& v=vertices[j];if(!composite_detail::point(ctx,p,storage[k],j,v)||v.color!=color)return false;
                v.x=cx+(v.x-cx)*native_scale;v.y=cy+(v.y-cy)*native_scale;
                const double q=kind==1?.625:.6289308071136475;
                if(std::abs(v.q-q)>1e-6||std::abs(v.x-cx)>(kind==1?120.591:123.051)||std::abs(v.y-cy)>(kind==1?112.701:115.001))return false;
                if(v.u<(kind==1?1./256:5./256)||v.u>(kind==1?255./256:251./256)||
                   v.v<(kind==1?0.:4./256)||v.v>(kind==1?254./256:250./256))return false;
                lo_x=std::min(lo_x,v.x);hi_x=std::max(hi_x,v.x);lo_y=std::min(lo_y,v.y);hi_y=std::max(hi_y,v.y);sum_x+=v.x-cx;sum_y+=v.y-cy;
                if(kind==1&&!composite_detail::center_uv(v)){
                    const double x=(v.x-cx)/120.5890197753906,y=(v.y-cy)/112.6999816894531;
                    if(std::abs(x*x+y*y-1)>.00003||std::abs(v.u-(.5+x*.5))>2./256||std::abs(v.v-(127./256+y*.5))>2./256)return false;
                }
            }
            if(kind==1){
                const auto angular_index=[&](const auto& v){
                    double angle=std::atan2((v.y-cy)/112.6999816894531,(v.x-cx)/120.5890197753906)*64/6.2831853071795864769;
                    if(angle<0)angle+=64;
                    const auto nearest=std::lround(angle);return std::abs(angle-nearest)<.0001?int(nearest%64):-1;
                };
                for(unsigned j=2;j<p.count;++j){const auto& a=vertices[j-2];const auto& b=vertices[j-1];const auto& c=vertices[j];
                    const composite_detail::Point* outer[2]{};unsigned n=0,centers=0;
                    for(const auto* v:{&a,&b,&c}){if(composite_detail::center_uv(*v))++centers;else if(n<2)outer[n++]=v;else return false;}
                    if(centers!=1||n!=2)return false;
                    const int ia=angular_index(*outer[0]),ib=angular_index(*outer[1]);if(ia<0||ib<0)return false;
                    int edge=-1;if((ib-ia+64)%64==1)edge=ia;else if((ia-ib+64)%64==1)edge=ib;
                    if(edge<0||edges[edge])return false;
                    edges[edge]=true;++triangles;
                }
            }else{
                const int sx=sum_x>=0?1:-1,sy=sum_y>=0?1:-1;const unsigned quadrant=(sx>0?2u:0u)+(sy>0?1u:0u);
                if(quadrants[quadrant])return false;
                quadrants[quadrant]=true;
                if(std::abs(std::max(std::abs(lo_x-cx),std::abs(hi_x-cx))-123.05)>.003||
                   std::abs(std::max(std::abs(lo_y-cy),std::abs(hi_y-cy))-115.)>.003)return false;
                for(unsigned j=0;j<p.count;++j)if((vertices[j].x-cx)*sx<-8||(vertices[j].y-cy)*sy<-8)return false;
                if(p.count==8)++rim8;else if(p.count==9)++rim9;else ++rim10;
            }
        }
        if(triangles!=64||!std::all_of(edges.begin(),edges.end(),[](bool edge){return edge;})||rim8!=1||rim9!=2||rim10!=1)return false;
        frame_=&ctx;generation_=ctx.dcCamera.generation;submit_=ctx.dcCamera.submit_sequence;storage_=storage;native_scale_=float(native_scale);valid_=true;return true;
    }
    template<class Context> bool matches_frame(const Context& ctx)const noexcept{
        return valid_&&frame_==&ctx&&generation_==ctx.dcCamera.generation&&submit_==ctx.dcCamera.submit_sequence&&
            !ctx.isRTT&&ctx.framebufferWidth==640&&ctx.framebufferHeight==480;
    }
    float native_scale()const noexcept{return native_scale_;}
    template<class Context> bool selected(const Context& ctx,std::size_t index)const noexcept{
        return matches_frame(ctx)&&
            index<ctx.global_param_tr.size()&&indices_.back()<ctx.global_param_tr.size()&&
            index>=indices_.front()&&index<=indices_.back()&&std::binary_search(indices_.begin(),indices_.end(),index);
    }
    template<class Context> bool contains_triangle(const Context& ctx,std::size_t polygon,uint32_t a,uint32_t b,uint32_t c)const noexcept{
        if(!selected(ctx,polygon)||a==b||a==c||b==c||a>=ctx.verts.size()||b>=ctx.verts.size()||c>=ctx.verts.size())return false;
        std::array<uint32_t,3> wanted{{a,b,c}};std::sort(wanted.begin(),wanted.end());
        const int kind=composite_detail::material(ctx.global_param_tr[polygon]);
        if(!kind)return false;
        for(unsigned k=0;k<26;++k){const auto& p=ctx.global_param_tr[indices_[k]];
            if(composite_detail::material(p)!=kind)continue;
            for(unsigned j=2;j<p.count;++j){std::array<uint32_t,3> actual{};
                for(unsigned i=0;i<3;++i){const uint64_t at=uint64_t(p.first)+j-2+i;
                    if(storage_[k]==PolygonStorage::Vertices)actual[i]=uint32_t(at);
                    else {if(at>=ctx.idx.size())return false;actual[i]=ctx.idx[at];}}
                std::sort(actual.begin(),actual.end());if(actual==wanted)return true;
            }
        }
        return false;
    }
    void clear()noexcept{*this=GeometryGroup{};}
private:
    const void* frame_{};uint64_t generation_{},submit_{};
    std::array<std::size_t,26> indices_{};std::array<PolygonStorage,26> storage_{};float native_scale_{};bool valid_{};
};
class CompositeMask {
public:
    template<class Context> bool configure(std::string_view product,const Context& ctx,bool vr_rifle_world,
        const TextureView& scope,uintptr_t context,double now)noexcept{
        clear();
        if(!vr_rifle_world||product!="T9507N"||ctx.isRTT||ctx.framebufferWidth!=640||ctx.framebufferHeight!=480||
           !ctx.dcCamera.generation||!ctx.dcCamera.submit_sequence||!context||!scope||scope.width!=256||scope.height!=256||
           !scope.content_top_at_v0||scope.context!=context||!scope.sequence||!scope.lifecycle_generation||
           scope.native_generation!=ctx.dcCamera.generation||!std::isfinite(now)||!std::isfinite(scope.captured_seconds)||
           scope.captured_seconds<0||now<scope.captured_seconds||now-scope.captured_seconds>.25||
           !scope.submit_sequence||scope.submit_sequence>ctx.dcCamera.submit_sequence||
           ctx.dcCamera.submit_sequence-scope.submit_sequence>4||ctx.global_param_tr.size()>16384||
           ctx.verts.size()>65536||ctx.idx.size()>262144)return false;
        if(!geometry_.configure(product,ctx))return false;
        context_=context;image_=scope;valid_=true;return true;
    }
    template<class Context> bool selected(const Context& ctx,std::size_t index)const noexcept{return valid_&&geometry_.selected(ctx,index);}
    const TextureView& image()const noexcept{return image_;}
    uintptr_t context()const noexcept{return context_;}
    const GeometryGroup& geometry_proof()const noexcept{return geometry_;}
    void clear()noexcept{*this=CompositeMask{};}
private:
    GeometryGroup geometry_;uintptr_t context_{};TextureView image_{};bool valid_{};
};
bool rifle_draw_authorized()noexcept;
bool set_rifle_draw_authorized(bool)noexcept;
// One freshness timestamp for the complete stereo presentation, including
// native-widget suppression and physical optic. Resets still invalidate live.
double presentation_seconds()noexcept;
double set_presentation_seconds(double)noexcept;
class ScopedRifleDrawAuthorization {
    bool previous_;
    double previous_seconds_;
public:
    explicit ScopedRifleDrawAuthorization(bool will_draw)noexcept:previous_(set_rifle_draw_authorized(will_draw)),
        previous_seconds_(set_presentation_seconds(presentation_seconds())){}
    ~ScopedRifleDrawAuthorization(){set_rifle_draw_authorized(previous_);set_presentation_seconds(previous_seconds_);}
    ScopedRifleDrawAuthorization(const ScopedRifleDrawAuthorization&)=delete;
    ScopedRifleDrawAuthorization& operator=(const ScopedRifleDrawAuthorization&)=delete;
};
inline CompositeMask& composite_draw_mask(){static thread_local CompositeMask mask;return mask;}
class ScopedCompositeDraw {
    CompositeMask previous_;
public:
    template<class Context> ScopedCompositeDraw(std::string_view product,const Context& ctx,bool vr_world,uintptr_t context)
        :previous_(composite_draw_mask()){
        const double now=presentation_seconds();
        const auto image=gpu_scope().acquire(ctx.dcCamera.submit_sequence,ctx.dcCamera.generation,context,now);
        composite_draw_mask().configure(product,ctx,vr_world&&rifle_draw_authorized(),image,context,now);
    }
    ~ScopedCompositeDraw(){composite_draw_mask()=previous_;}
    ScopedCompositeDraw(const ScopedCompositeDraw&)=delete;
    ScopedCompositeDraw& operator=(const ScopedCompositeDraw&)=delete;
};
template<class Context> bool should_skip_scope_composite(const Context& ctx,std::size_t polygon_index)noexcept{
    const auto& mask=composite_draw_mask();
    if(!rifle_draw_authorized()||!mask.selected(ctx,polygon_index))return false;
    const auto live=gpu_scope().acquire(ctx.dcCamera.submit_sequence,ctx.dcCamera.generation,mask.context(),presentation_seconds());
    return live&&live.sequence==mask.image().sequence&&live.lifecycle_generation==mask.image().lifecycle_generation&&live.texture==mask.image().texture;
}
}
