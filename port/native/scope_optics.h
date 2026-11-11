// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>

namespace silent_scope {
struct Vec2 {double x{},y{};};
struct Vec3 {double x{},y{},z{};};
struct ScopeGeometry {
    // Eye relative to ocular center, rifle barrel points along local -Z, metres.
    double eye_box_radius{.025}, minimum_relief{.015}, maximum_relief{.30};
};
struct ScopeVisibility {bool visible{};};
inline ScopeVisibility scope_visibility(Vec3 eye,const ScopeGeometry& scope) {
    if(!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z)||
       !std::isfinite(scope.eye_box_radius)||scope.eye_box_radius<=0||scope.eye_box_radius>.1||
       !std::isfinite(scope.minimum_relief)||scope.minimum_relief<0||
       !std::isfinite(scope.maximum_relief)||scope.maximum_relief<=scope.minimum_relief) return {};
    return {eye.z>=scope.minimum_relief && eye.z<=scope.maximum_relief &&
            std::hypot(eye.x,eye.y)<=scope.eye_box_radius};
}
struct ScopeUV {double u{},v{};bool valid{};};
// Source is top-row-first, local lens coordinates are Y-up. Aspect-correct center
// crop fills the circular optic without stretching source pixels. The renderer
// masks outside the physical aperture; source crosshair center stays at UV .5/.5.
inline ScopeUV scope_uv(Vec2 lens,double display_aspect) {
    if(!std::isfinite(lens.x)||!std::isfinite(lens.y)||
       !std::isfinite(display_aspect)||display_aspect<.25||display_aspect>4) return {};
    return {.5+lens.x/std::max(1.0,display_aspect),
            .5-lens.y*std::min(1.0,display_aspect),true};
}
}
