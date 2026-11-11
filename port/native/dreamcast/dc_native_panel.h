// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/glm.hpp>
#include <cmath>
#include <string_view>
namespace silent_scope::dc_native_panel {
// Reserved negative value: native W is positive. The shader flattens raster XY
// while retaining the original W varying used by native fragment depth.
constexpr float FlatMarker=-2.f;
inline float depth(float metres_per_unit,float panel_metres){
    if(!std::isfinite(metres_per_unit)||metres_per_unit<1e-6f)return 0;
    if(!std::isfinite(panel_metres)||panel_metres<.5f||panel_metres>10.f)panel_metres=1.8f;
    const float result=panel_metres/metres_per_unit;return std::isfinite(result)?result:0;
}
template<class Params> bool apply(std::string_view title,bool is_rtt,unsigned width,unsigned height,bool xr_eye,bool reproject,
        float metres_per_unit,float panel_metres,const glm::mat4& eye_view_projection,Params& p){
    const float z=depth(metres_per_unit,panel_metres);
    if(title!="T9507N"||is_rtt||width!=640||height!=480||!xr_eye||!reproject||z<=0||
       !std::isfinite(p.tanHalf.x)||!std::isfinite(p.tanHalf.y)||p.tanHalf.x<=0||p.tanHalf.y<=0)return false;
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)if(!std::isfinite(eye_view_projection[col][row]))return false;
    p.overlay=glm::vec4(p.tanHalf,z,FlatMarker);p.viewProj=eye_view_projection;return true;
}
}
