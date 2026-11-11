// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "projection.h"
#include <algorithm>
#include <array>
namespace silent_scope::scene {
inline constexpr float MaxStereoSeparation=5.25f;
inline constexpr std::array<float,6> StereoSeparationSteps{1,1.5f,2,3,4,MaxStereoSeparation};
inline float next_separation(float value,int direction){if(direction>=0){for(auto x:StereoSeparationSteps)if(x>value+.0001f)return x;return MaxStereoSeparation;}for(auto i=StereoSeparationSteps.rbegin();i!=StereoSeparationSteps.rend();++i)if(*i<value-.0001f)return *i;return 1;}
// Presentation equivalent to virtual eyes separated by gain g. Cyclopean
// angular size is preserved, but binocularly inferred size is reduced by1/g.
struct StereoPresentation {
 Vec3 center{};double separation{1};
 double factor()const{return finite(center)&&std::isfinite(separation)?std::clamp(separation,1.,double(MaxStereoSeparation)):1.;}
 Vec3 to_display(Vec3 point)const{const auto g=factor();return g==1?point:center+(point-center)*(1/g);}
 Ray to_world(Ray ray)const{const auto g=factor();return g==1?ray:Ray{center+(ray.origin-center)*g,ray.direction*g};}
};
}

