// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace silent_scope {
class RifleScopeStability {
    glm::vec3 previousCheek_{}, cheek_{}, cheekVelocity_{}, angularVelocity_{};
    glm::quat previousRotation_{1,0,0,0}, rotation_{1,0,0,0};
    double previousTime_{-1};
    float blend_{};

    static float alpha(double hz, double dt) {
        return float(-std::expm1(-6.283185307179586 * hz * dt));
    }

public:
    void reset() { previousTime_ = -1; blend_ = 0; }

    glm::mat4 update(const glm::mat4& pose, glm::vec3 head, glm::vec3 ocular,
                     double now, float alignment, int level) {
        if (level <= 0 || !std::isfinite(now) || !std::isfinite(alignment)) {
            reset();
            return pose;
        }
        const auto rawCheek = glm::vec3(pose * glm::vec4(ocular, 1)) - head;
        auto rawRotation = glm::normalize(glm::quat_cast(glm::mat3(pose)));
        const double dt = now - previousTime_;
        const bool seed = previousTime_ < 0 || dt <= 0 || dt > .25;
        previousTime_ = now;
        if (seed || (blend_ < .001f && alignment <= 0.f)) {
            previousCheek_ = cheek_ = rawCheek;
            previousRotation_ = rotation_ = rawRotation;
            cheekVelocity_ = angularVelocity_ = glm::vec3(0);
            blend_ = 0;
            return pose;
        }

        // Average signed velocity before taking its magnitude. An intentional
        // sweep stays coherent while alternating tremor largely cancels out.
        if (glm::dot(previousRotation_, rawRotation) < 0)
            rawRotation = -rawRotation;
        const auto delta = rawRotation * glm::conjugate(previousRotation_);
        const auto axis = glm::vec3(delta.x, delta.y, delta.z);
        const float sine = glm::length(axis);
        const float rate = sine > 1e-7f ? 2.f * std::atan2(sine, delta.w) / (sine * float(dt)) : 0.f;
        const float velocityAlpha = alpha(1.5, dt);
        cheekVelocity_ = glm::mix(cheekVelocity_, (rawCheek - previousCheek_) / float(dt), velocityAlpha);
        angularVelocity_ = glm::mix(angularVelocity_, axis * rate, velocityAlpha);
        previousCheek_ = rawCheek;
        previousRotation_ = rawRotation;

        constexpr float idleCutoff[]{3.0f, 1.9f, 1.15f, .75f, .48f};
        const float cutoff = idleCutoff[std::min(level, 5) - 1];
        const float travelSpeed = std::max(0.f, glm::length(cheekVelocity_) - .028f);
        const float turnSpeed = std::max(0.f, glm::length(angularVelocity_) - .045f);
        const float positionHz = std::min(45.f, cutoff + 90.f * travelSpeed);
        const float rotationHz = std::min(45.f, cutoff + 19.f * turnSpeed);
        cheek_ = glm::mix(cheek_, rawCheek, alpha(positionHz, dt));
        rotation_ = glm::normalize(glm::slerp(rotation_, rawRotation, alpha(rotationHz, dt)));

        // No captured shoulder point. The rest floats with the rifle, engages
        // gradually, and yields continuously during a large reposition.
        blend_ += (std::clamp(alignment, 0.f, 1.f) - blend_) * alpha(1.6, dt);
        const float drift = glm::length(cheek_ - rawCheek);
        const float contact = blend_ * (1.f - glm::smoothstep(.010f, .045f, drift));
        const auto rotationError = glm::conjugate(rawRotation) * rotation_;
        const float angle = 2.f * std::atan2(glm::length(glm::vec3(rotationError.x, rotationError.y, rotationError.z)), std::abs(rotationError.w));
        const float angularContact = contact * (1.f - glm::smoothstep(.025f, .14f, angle));
        auto result = glm::mat4_cast(glm::normalize(glm::slerp(rawRotation, rotation_, angularContact)));
        const auto stableCheek = head + glm::mix(rawCheek, cheek_, contact);
        result[3] = glm::vec4(stableCheek - glm::mat3(result) * ocular, 1);
        return result;
    }
};
}
