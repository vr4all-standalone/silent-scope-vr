// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rifle_input.h"
#include "scope_optics.h"
#include <openxr/openxr.h>
#include <numbers>

namespace silent_scope {
inline bool view_tracking_good(XrViewStateFlags flags){
    constexpr auto required=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT|
                            XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT;
    return (flags&required)==required;
}
inline HandSample tracked_hand(XrSpaceLocationFlags flags,Vec3 forward,bool trigger){
    constexpr auto required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|
                            XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
    double length=std::hypot(forward.x,forward.y,forward.z);
    if((flags&required)!=required||!std::isfinite(length)||length<1e-6)return {};
    // Native X increases toward screen-right; OpenXR rifle forward is local -Z.
    return {true,std::atan2(forward.x,-forward.z)*180/std::numbers::pi,
            std::asin(std::clamp(forward.y/length,-1.0,1.0))*180/std::numbers::pi,trigger};
}
}
