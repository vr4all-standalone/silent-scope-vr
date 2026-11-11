// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_camera_focal_probe.h"
#include <atomic>
#include <cstring>

namespace silent_scope::dreamcast {
inline std::atomic<float> scope_camera_magnification{1.f};
inline constexpr uint32_t ScopeWorldFocalBits = 0x465363f2;
inline constexpr uint32_t IndoorScopeWorldFocalBits = 0x461a8a85;
inline constexpr uint32_t ScopeSourceAddress = 0x8c1b010c;
inline constexpr bool known_scope_world_focal(uint32_t bits) {
    return bits == ScopeWorldFocalBits || bits == IndoorScopeWorldFocalBits;
}

inline FocalSetterDecision decide_scope_camera(float scale, uint32_t loaded, uint32_t caller,
        uint32_t mode, uint32_t fpscr, bool guard, bool fpu_disabled) {
    FocalSetterDecision result{loaded, false, false};
    if (!guard || fpu_disabled || caller != FocalSetterProbe::CallerReturn || mode != 1 ||
        !known_scope_world_focal(loaded) || (fpscr & ((1u << 19) | (1u << 20))) ||
        (scale != .2f && scale != .5f && scale != 1.f && scale != 2.f && scale != 5.f))
        return result;
    result.eligible = true;
    result.applied = scale != 1.f;
    float focal;
    std::memcpy(&focal, &loaded, sizeof focal);
    focal *= scale;
    std::memcpy(&result.effective_bits, &focal, sizeof focal);
    return result;
}
}
