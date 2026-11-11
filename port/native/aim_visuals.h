// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "settings.h"
#include "scene/scene_geometry.h"
#include "shared/xr_panel.h"
#include <optional>

namespace silent_scope {
struct AimHit {glm::vec3 point{};double distance{};};
// Geometry/visual adapter only: this never changes native potentiometers or
// claims a confirmed shot/hit. Native pot->raster correspondence still needs
// matched native scope/headset evidence; its linear mapping is explicit below.
class AimVisuals {
public:
    void prepare(std::shared_ptr<const scene::RawFrame> frame,const Settings& settings,
                 const glm::mat4& active_calibrated_model,bool allowed);
    const std::optional<AimHit>& hit() const{return hit_;}
    glm::vec3 beam_origin() const{return muzzle_;}
    double reticle_radius() const{return radius_;}
    static double reticle_reference(double distance,const Settings& settings);
    void draw(const glm::mat4& view_projection,glm::vec3 eye_position) const;
    // Overlay pointer; independently gated by focus/tracking/panel intersection,
    // never by gameplay Laser or Reticle preferences. Caller supplies a real hit.
    static void draw_menu_pointer(const glm::mat4& view_projection,glm::vec3 origin,
                                  glm::vec3 hit,bool active,glm::vec3 eye_position={0,0,0});
private:
    std::shared_ptr<const scene::RawFrame> cached_frame_;
    double cached_scale_{};
    std::vector<std::array<scene::Vec3,3>> triangles_;
    std::optional<AimHit> hit_;
    glm::vec3 muzzle_{};
    double radius_{};bool laser_{};
    void cache_geometry(std::shared_ptr<const scene::RawFrame>,double scale);
};
}
