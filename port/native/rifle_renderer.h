// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "display_bridge.h"
#include "rifle_mesh.h"
#include <glm/glm.hpp>
#include <memory>

namespace silent_scope {
class RifleRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    RifleRenderer();
    explicit RifleRenderer(const RifleMesh& mesh);
    ~RifleRenderer();
    void draw_cartridge(const glm::mat4& view_projection,const glm::mat4& model,bool spent);
    void draw_magazine(const glm::mat4& view_projection,const glm::mat4& model,bool highlighted=false);
    void draw_support_box(const glm::mat4& view_projection,const glm::mat4& model,bool guide=false);
    void draw_panel(const glm::mat4& view_projection,std::shared_ptr<const RasterFrame> frame,float distance);
    void draw_rifle(const glm::mat4& view_projection,const glm::mat4& rifle_model,
                    glm::vec3 eye_world,std::shared_ptr<const RasterFrame> scope);
    // Borrowed render-thread texture; caller retains ownership and passes0 when
    // unavailable. Native OpenGL RTT images use bottom-left origin.
    void draw_rifle_texture(const glm::mat4& view_projection,const glm::mat4& rifle_model,
                           glm::vec3 eye_world,uint32_t scope_texture,float aspect,bool bottom_left=true,
                           float image_offset=0.f,float image_scale=1.f,
                           float bolt_offset=0.f,const glm::mat4* magazine_model=nullptr,bool magazine_visible=true,float bolt_lift=0.f,
                           bool highlight_bolt=false,bool highlight_magazine=false);
};
}
