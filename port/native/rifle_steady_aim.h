// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace silent_scope {
class RifleSteadyAim {
public:
    void reset() { *this = RifleSteadyAim{}; }

    glm::mat4 update(const glm::mat4& pose, glm::vec3 ocular, double now, bool grip, bool tracked) {
        if (!tracked || !std::isfinite(now)) {
            reset();
            return pose;
        }
        if (!grip)
            armed_ = true;
        if (!armed_)
            return pose;
        if (!grip && !held_ && (release_time_ < 0 || now - release_time_ >= Recovery)) {
            release_time_ = -1;
            return pose;
        }

        const auto raw = glm::normalize(glm::quat_cast(glm::mat3(pose)));
        if (grip && !held_) {
            anchor_ = raw;
            engaged_ = release_offset(now) * raw;
        } else if (!grip && held_) {
            offset_ = last_ * glm::conjugate(raw);
            release_time_ = now;
        }
        held_ = grip;
        last_ = grip ? glm::normalize(glm::slerp(anchor_, raw, .2f) * glm::conjugate(anchor_) * engaged_)
                     : glm::normalize(release_offset(now) * raw);
        auto result = glm::mat4_cast(last_);
        result[3] = glm::vec4(glm::vec3(pose * glm::vec4(ocular, 1)) - glm::mat3(result) * ocular, 1);
        return result;
    }

private:
    static constexpr double Recovery = .18;
    static constexpr glm::quat Identity{1, 0, 0, 0};
    glm::quat release_offset(double now) const {
        if (release_time_ < 0 || now - release_time_ >= Recovery)
            return Identity;
        return glm::slerp(offset_, Identity, glm::smoothstep(0.f, 1.f, float((now - release_time_) / Recovery)));
    }
    glm::quat anchor_ = Identity, engaged_ = Identity, offset_ = Identity, last_ = Identity;
    double release_time_ = -1;
    bool armed_ = false, held_ = false;
};
}
