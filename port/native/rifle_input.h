// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace silent_scope {
struct Calibration {
    double yaw_center{},pitch_center{},yaw_half_range{45},pitch_half_range{30};
    bool valid() const {
        return std::isfinite(yaw_center)&&std::isfinite(pitch_center)&&
               std::isfinite(yaw_half_range)&&yaw_half_range>=1&&yaw_half_range<=180&&
               std::isfinite(pitch_half_range)&&pitch_half_range>=1&&pitch_half_range<=90;
    }
};
struct HandSample {bool tracked{};double yaw_degrees{},pitch_degrees{};bool trigger{};};
struct InputContext {bool focused{true},menu{};int active_hand{1};};
struct CabinetInput {uint16_t yaw{1024},pitch{1024};bool trigger{};};
class RifleInput {
    int hand_{-1};bool armed_{};
public:
    void reset() {hand_=-1;armed_=false;}
    CabinetInput update(HandSample left,HandSample right,InputContext context,const Calibration& calibration) {
        if(context.active_hand!=hand_) {armed_=false;hand_=context.active_hand;}
        if(context.active_hand<0||context.active_hand>1) {armed_=false;return {};}
        const auto& hand=context.active_hand?right:left;
        if(!context.focused||context.menu||!hand.tracked||!calibration.valid()||
           !std::isfinite(hand.yaw_degrees)||!std::isfinite(hand.pitch_degrees)) {
            armed_=false;return {};
        }
        if(!hand.trigger) armed_=true;
        auto map=[](double normalized) {return uint16_t(std::lround(std::clamp(normalized,0.0,1.0)*2047));};
        return {map(.5+std::remainder(hand.yaw_degrees-calibration.yaw_center,360.0)/(2*calibration.yaw_half_range)),
                map(.5+(hand.pitch_degrees-calibration.pitch_center)/(2*calibration.pitch_half_range)),
                armed_&&hand.trigger};
    }
};
}
