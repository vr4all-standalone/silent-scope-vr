// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace silent_scope { namespace dreamcast {
struct CameraContext {
    uint64_t generation{},submit_sequence{};
    double native_seconds{};
    uint32_t ta_address{},destination{};
    uint32_t story_scene{};
    uint16_t story_stage{0xffff};
    uint16_t story_route{0xffff};
    bool supported{},is_rtt{},candidates_captured{},code_guard_valid{};
    bool cache_captured{};
    float cache_fx{},cache_fy{},cache_cx{},cache_cy{};
    uint32_t failed_guard_address{},failed_guard_expected{},failed_guard_actual{};
    std::array<uint32_t,44> sdk_words{};
    std::array<uint32_t,12> object_words{};
};
}}
