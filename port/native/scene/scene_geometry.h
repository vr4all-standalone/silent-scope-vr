// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"
#include "depth_strength.h"
#include "stereo_separation.h"
#include <algorithm>
namespace silent_scope::scene {
// Presentation scale: native foreground depths around 1500 become about 4m.
// The native camera projection is unchanged; binocular and positional depth
// remain geometric rather than a flat raster effect.
inline constexpr float SceneMetresPerUnit=.0025f;
inline bool scene_panel_triangle(const RawTriangle& triangle){
    return std::all_of(triangle.vertex.begin(),triangle.vertex.end(),[](const auto& vertex){return std::abs(vertex.q-1./1.1)<1e-5;});
}
inline NativeLens presentation_lens(const RawFrame& frame){
    if(frame.lens.valid())return frame.lens;
    return {int(frame.width),int(frame.height),double(frame.width),double(frame.width),frame.width*.5,frame.height*.5};
}
inline double background_units(const RawFrame& frame){
    double depth=50;for(const auto& triangle:frame.triangles)for(const auto& vertex:triangle.vertex)
        if(vertex.q>1e-5)depth=std::max(depth,1.05/vertex.q);
    return depth;
}
}
