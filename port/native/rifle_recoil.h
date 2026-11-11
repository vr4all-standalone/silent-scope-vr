// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/gtc/matrix_transform.hpp>

namespace silent_scope {
class RifleRecoil {
public:
    void reset() { shot_time_ = -1; }
    void shot(double now, bool braced) {
        shot_time_ = now;
        braced_ = braced;
    }
    glm::mat4 apply(const glm::mat4& model, double now) const {
        const double age = now - shot_time_;
        constexpr double rise = .025;
        constexpr double settle = .22;
        if (shot_time_ < 0 || age < 0 || age >= settle)
            return model;
        const double recovery = (settle - age) / (settle - rise);
        const float kick = float(age < rise ? age / rise : recovery * recovery) * (braced_ ? 1.f : 2.f);
        const auto translated = glm::translate(model, glm::vec3(0, 0, kick * .018f));
        return glm::rotate(translated, glm::radians(kick * 4.5f), glm::vec3(1, 0, 0));
    }
private:
    double shot_time_ = -1;
    bool braced_ = false;
};
}
