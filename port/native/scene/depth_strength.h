// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace silent_scope::scene {
inline constexpr float DefaultDepthStrength=1.5f,MaxDepthStrength=2.f;
inline double scene_depth_strength(double value){return std::isfinite(value)?std::clamp(value,1.,double(MaxDepthStrength)):1.;}
}

