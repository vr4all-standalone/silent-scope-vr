// SPDX-License-Identifier: GPL-2.0-or-later
#include "aim_visuals.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <numbers>

namespace silent_scope {
namespace {
bool finite(glm::vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
bool valid_model(const glm::mat4& model) {
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(model[c][r]))return false;
    if(std::abs(model[3][3]-1)>1e-4f)return false;
    for(int i=0;i<3;++i) {
        if(std::abs(model[i][3])>1e-4f||std::abs(glm::dot(glm::vec3(model[i]),glm::vec3(model[i]))-1)>1e-3f)return false;
        for(int j=i+1;j<3;++j)if(std::abs(glm::dot(glm::vec3(model[i]),glm::vec3(model[j])))>1e-3f)return false;
    }
    return glm::determinant(glm::mat3(model))>.999f;
}
glm::mat4 billboard(glm::vec3 center,glm::vec3 eye) {
    auto normal=eye-center;if(glm::length(normal)<1e-6f)normal={0,0,1};else normal=glm::normalize(normal);
    auto right=glm::cross(glm::vec3(0,1,0),normal);
    if(glm::length(right)<1e-6f)right={1,0,0};else right=glm::normalize(right);
    auto up=glm::normalize(glm::cross(normal,right));
    glm::mat4 model(1);model[0]=glm::vec4(right,0);model[1]=glm::vec4(up,0);model[2]=glm::vec4(normal,0);model[3]=glm::vec4(center,1);return model;
}
void line(const glm::mat4& vp,glm::vec3 from,glm::vec3 to,glm::vec3 eye,float width,const glm::vec4& colour,bool depth_test) {
    const auto delta=to-from;const float length=glm::length(delta);
    if(!finite(from)||!finite(to)||!finite(eye)||!std::isfinite(length)||length<1e-6f)return;
    const auto middle=(from+to)*.5f,along=delta/length;
    auto right=glm::cross(along,eye-middle);
    if(glm::length(right)<1e-6f)right=glm::cross(along,glm::vec3(0,1,0));
    if(glm::length(right)<1e-6f)right=glm::cross(along,glm::vec3(1,0,0));
    right=glm::normalize(right);const auto normal=glm::normalize(glm::cross(right,along));
    glm::mat4 model(1);model[0]=glm::vec4(right,0);model[1]=glm::vec4(along,0);model[2]=glm::vec4(normal,0);model[3]=glm::vec4(middle,1);
    vr::xr::drawTexture(vp,model,0,width*.5f,length*.5f,colour,depth_test);
}
}
double AimVisuals::reticle_reference(double distance,const Settings& s) {
    if(!std::isfinite(distance)||distance<0||!std::isfinite(s.panel_distance)||s.panel_distance<=0||
       !std::isfinite(s.reticle_min_size)||s.reticle_min_size<.05||s.reticle_min_size>1||
       !std::isfinite(s.reticle_max_size)||s.reticle_max_size<.25||s.reticle_max_size>3||
       !std::isfinite(s.reticle_shrink_distance)||s.reticle_shrink_distance<=0)return 0;
    const double reference=distance<=s.panel_distance?s.panel_distance:
        distance*std::max(s.reticle_min_size,1/(1+(distance-s.panel_distance)/s.reticle_shrink_distance));
    return reference*s.reticle_max_size;
}
void AimVisuals::cache_geometry(std::shared_ptr<const scene::RawFrame> frame,double scale) {
    if(cached_frame_==frame&&cached_scale_==scale)return;
    cached_frame_=std::move(frame);cached_scale_=scale;triangles_.clear();
    if(!cached_frame_||!cached_frame_->complete||!cached_frame_->lens.valid())return;
    const auto& f=*cached_frame_;triangles_.reserve(f.triangles.size());
    for(const auto& triangle:f.triangles) {
        if(triangle.overlay||scene::scene_panel_triangle(triangle))continue;
        std::array<scene::Vec3,3> vertices;bool valid=true;
        for(int i=0;i<3;++i) {
            const auto& v=triangle.vertex[i];
            if(!std::isfinite(v.q)||v.q<=0){valid=false;break;}
            const double depth=1/std::max(v.q,.00001);
            const double y=(triangle.state.depth_mode&(1u<<17))?f.height-v.y:v.y;
            vertices[i]=f.lens.unproject(v.x,y,depth*scale);
            if(!scene::finite(vertices[i])){valid=false;break;}
        }
        if(valid)triangles_.push_back(vertices);
    }
}
void AimVisuals::prepare(std::shared_ptr<const scene::RawFrame> frame,const Settings& settings,const glm::mat4& model,bool allowed) {
    hit_.reset();radius_=0;laser_=settings.laser_on;
    if(!allowed||!frame||!frame->complete||!frame->lens.valid()||frame->width!=uint32_t(frame->lens.width)||frame->height!=uint32_t(frame->lens.height)||
       !valid_model(model)||!settings.calibration.valid()||
       !std::isfinite(settings.world_size)||settings.world_size<.01||settings.world_size>1)return;
    muzzle_=glm::vec3(model*glm::vec4(0,.005f,-.94f,1)); // rifle_mesh.h actual open barrel front
    const double scale=scene::SceneMetresPerUnit*settings.world_size/.1;
    cache_geometry(frame,scale);
    if(triangles_.empty())return;
    auto stock=frame->lens;
    if(frame->wider_applied) {
        if(!std::isfinite(frame->wider_factor)||frame->wider_factor<1||frame->wider_factor>scene::WiderView::MaxWideFactor)return;
        stock.fx*=frame->wider_factor;stock.fy*=frame->wider_factor;
    }
    // Current measured stock focal length is443, even when native main-camera
    // output is widened. Reconstruct it from this buffer's immutable metadata,
    // never from a requested setting or the currently live DSP lens.
    const auto forward=glm::normalize(glm::vec3(model*glm::vec4(0,0,-1,0)));
    const double yaw=std::atan2(forward.x,-forward.z)*180/std::numbers::pi;
    const double pitch=std::asin(std::clamp(double(forward.y),-1.,1.))*180/std::numbers::pi;
    RifleInput mapping;const auto pot=mapping.update({},HandSample{true,yaw,pitch,false},{true,false,1},settings.calibration);
    // Inferred native pot->viewport mapping, awaiting native scope validation.
    // Positive physical pitch already maps to increasing native pots; invert
    // raster Y here to retain the accepted physical-up direction.
    const auto target=stock.unproject(double(pot.yaw)/2047*frame->width,(1-double(pot.pitch)/2047)*frame->height,1);
    if(!scene::finite(target))return;
    const double magnitude=std::sqrt(scene::dot(target,target));if(!std::isfinite(magnitude)||magnitude<1e-9)return;
    const scene::Ray ray{{0,0,0},target*(1/magnitude)};
    double nearest=std::numeric_limits<double>::infinity();
    for(const auto& triangle:triangles_)if(auto point=scene::intersect_triangle(ray,triangle)) {
        const auto distance=std::sqrt(scene::dot(*point,*point));
        if(distance<nearest){nearest=distance;hit_=AimHit{glm::vec3(point->x,point->y,point->z),distance};}
    }
    if(hit_&&settings.reticle==Reticle::Custom)radius_=.012*reticle_reference(hit_->distance,settings);
}
void AimVisuals::draw(const glm::mat4& vp,glm::vec3 eye) const {
    if(!hit_||!finite(eye))return;
    const glm::vec4 p1(1,.12f,.08f,1);
    if(laser_)line(vp,muzzle_,hit_->point,eye,.0014f,glm::vec4(p1.x,p1.y,p1.z,.78f),true);
    if(radius_<=0||!std::isfinite(radius_))return;
    // An outlined X rather than a filled cross/dot. All geometry uses the real
    // target plane; a0.5mm visual-only offset avoids coplanar depth flicker.
    auto toward=eye-hit_->point;if(glm::length(toward)<1e-6f)return;toward=glm::normalize(toward);
    const auto model=billboard(hit_->point+toward*.0005f,eye);
    constexpr glm::vec2 outline[]{{-1,-.7f},{-.7f,-1},{0,-.3f},{.7f,-1},{1,-.7f},{.3f,0},
                                {1,.7f},{.7f,1},{0,.3f},{-.7f,1},{-1,.7f},{-.3f,0}};
    const auto point=[&](glm::vec2 p){return glm::vec3(model*glm::vec4(p*float(radius_),0,1));};
    for(int i=0;i<12;++i)line(vp,point(outline[i]),point(outline[(i+1)%12]),eye,float(radius_)*.055f,p1,true);
}
void AimVisuals::draw_menu_pointer(const glm::mat4& vp,glm::vec3 origin,glm::vec3 hit,bool active,glm::vec3 eye) {
    if(!active||!finite(origin)||!finite(hit)||!finite(eye)||glm::length(hit-origin)<1e-6f)return;
    const auto colour=glm::vec4(vr::xr::theme::Accent,1);
    line(vp,origin,hit,eye,.0018f,colour,false);
    vr::xr::drawTexture(vp,billboard(hit,eye),0,.003f,.003f,colour,false);
}
}
