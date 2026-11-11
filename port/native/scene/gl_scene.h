// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"

#include "stereo_separation.h"
#include <glm/glm.hpp>
#include <memory>
namespace silent_scope::scene {
class BackgroundSurface;
class GLScene {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    GLScene();~GLScene();
    void prepare(std::shared_ptr<const RawFrame> frame,bool depth_background=false);
    const BackgroundSurface* depth_surface(const std::shared_ptr<const RawFrame>& frame,bool depth_background) const;

    void draw(const glm::mat4& view_projection,float world_scale,float panel_distance,float depth_strength=1,const StereoPresentation& stereo=StereoPresentation{},float hud_size=1);
};
}

