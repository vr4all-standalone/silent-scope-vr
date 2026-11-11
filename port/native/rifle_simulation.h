// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rifle_support.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace silent_scope {
class RifleCarry {
public:
    enum class Hold { Chest, Rear, Front };
    static constexpr float RearRegrabRadius = .065f;

    void reset() { *this = RifleCarry{}; }
    Hold hold() const { return hold_; }
    bool rejoined_front() const { return rejoined_front_; }
    void remember_aimed_pose(const glm::mat4& pose, bool braced) {
        if (hold_ != Hold::Rear) return;
        model_ = pose;
        braced_ = braced;
    }

    glm::mat4 update(const glm::mat4& chest, const glm::mat4& rear, const glm::mat4& front,
                     RifleForegrip foregrip, bool rear_grip, bool front_grip, float size=1.f) {
        rejoined_front_ = false;
        if (hold_ == Hold::Chest) model_ = chest;
        if (hold_ == Hold::Front && front_grip) model_ = front * front_offset_;

        if (hold_ == Hold::Rear && !rear_grip) {
            if (front_grip && braced_) attach_front(front);
            else stow(chest);
        } else if (hold_ == Hold::Front && !front_grip) {
            stow(chest);
        }

        if (hold_ != Hold::Rear && rear_grip &&
            glm::distance(glm::vec3(rear[3]),glm::vec3(model_[3])) < (hold_ == Hold::Front ? RearRegrabRadius : RifleSupport::CaptureRadius)*size) {
            rejoined_front_ = hold_ == Hold::Front;
            rear_offset_ = rejoined_front_ ? glm::inverse(rear) * model_ : glm::mat4(1.f);
            hold_ = Hold::Rear;
            if (rejoined_front_) return model_;
        } else if (hold_ == Hold::Chest && front_grip) {
            const auto local = glm::vec3(glm::inverse(model_) * front[3]);
            const glm::vec3 contact{0,foregrip.height,std::clamp(local.z,foregrip.front,foregrip.rear)};
            if (glm::distance(local,contact) < RifleSupport::CaptureRadius*size) attach_front(front);
        }

        return hold_ == Hold::Rear ? rear * rear_offset_ : model_;
    }

private:
    void attach_front(const glm::mat4& front) {
        front_offset_ = glm::inverse(front) * model_;
        hold_ = Hold::Front;
    }
    void stow(const glm::mat4& chest) {
        model_ = chest;
        hold_ = Hold::Chest;
    }

    Hold hold_ = Hold::Chest;
    bool braced_ = false, rejoined_front_ = false;
    glm::mat4 model_{1.f}, front_offset_{1.f}, rear_offset_{1.f};
};

class RifleSimulation {
public:
    static constexpr int Capacity = 5;
    static constexpr glm::vec3 BoltHandle{.050f,.063f,-.045f};
    static constexpr glm::vec3 BoltAxis{0,.066f,0};
    static constexpr glm::vec3 EjectionPort{.015f,.077f,-.025f};
    static constexpr glm::vec3 MagazineBase{0,-.018f,-.112f};
    enum class Magazine { Seated, EmptyInHand, Missing, FreshInHand };

    void reset() { *this = RifleSimulation{}; }
    // A loaded physical magazine mirrors the native gauge. Once empty, only
    // physically seating a replacement may reveal the native refill again.
    void sync_native_rounds(int rounds) {
        if (magazine_ == Magazine::Seated && rounds_ > 0 && rounds >= 1 && rounds <= Capacity)
            rounds_ = rounds;
    }
    static bool native_shot_spent(int before, int after) {
        return before > 0 && after >= 0 && (after < before || (before == 1 && after == Capacity));
    }
    void release_hand() {
        grip_was_ = true;
        holding_bolt_ = false;
        if (magazine_ != Magazine::Seated) magazine_ = Magazine::Missing;
    }
    void update(glm::vec3 hand, bool grip, bool tracked, bool near_belt) {
        if (!tracked) {
            release_hand();
            return;
        }
        const bool grab = grip && !grip_was_;
        grip_was_ = grip;
        if (!grip && !holding_bolt_) {
            if (magazine_ != Magazine::Seated) magazine_ = Magazine::Missing;
            return;
        }
        if (grab && magazine_ == Magazine::Missing && near_belt)
            magazine_ = Magazine::FreshInHand;
        if (magazine_ == Magazine::FreshInHand && glm::distance(hand, MagazineBase) < .06f) {
            magazine_ = Magazine::Seated;
            rounds_ = Capacity;
        }
        if (grab && magazine_ == Magazine::Seated && rounds_ == 0 &&
            glm::distance(hand, MagazineBase) < .07f)
            magazine_ = Magazine::EmptyInHand;
        if (chambered_ && !holding_bolt_ && lift_ <= .15f) return;
        const auto handle=bolt_handle();
        if (grip && !holding_bolt_ && magazine_ != Magazine::EmptyInHand &&
            magazine_ != Magazine::FreshInHand && glm::distance(hand, handle) < .075f) {
            holding_bolt_ = true;
            grab_z_ = hand.z - bolt_;
            grab_y_ = hand.y - .05f*lift_;
        }
        if (!holding_bolt_) return;
        constexpr float Raised=.95f;
        const float requested_lift=std::clamp((hand.y-grab_y_)/.05f,0.f,Raised);
        if(bolt_<=.012f) lift_=requested_lift;
        if(!chambered_ && (lift_>=.8f||bolt_>.012f)) bolt_=std::clamp(hand.z-grab_z_,0.f,.09f);
        if (bolt_ >= .065f && !opened_) {
            opened_ = true;
            if (spent_case_) {
                spent_case_ = false;
                ++ejections_;
            }
        }
        if (opened_ && bolt_ <= .012f) {
            chambered_ = magazine_ == Magazine::Seated && rounds_ > 0;
            opened_ = false;
            bolt_ = 0;
        }
        if (!grip) holding_bolt_ = false;
        if (chambered_ && bolt_ == 0 && lift_ <= .15f) {
            holding_bolt_ = false;
            lift_ = 0;
        }
    }
    bool can_fire() const { return chambered_ && bolt_ <= .004f && magazine_ == Magazine::Seated && rounds_ > 0; }
    void fired() { if (can_fire()) { --rounds_; chambered_ = false; spent_case_ = true; } }
    unsigned ejections() const { return ejections_; }
    bool feeding() const { return opened_ && magazine_ == Magazine::Seated && rounds_ > 0; }
    glm::vec3 bolt_handle() const {
        const auto arm=BoltHandle-BoltAxis;
        return BoltAxis+glm::vec3(
            arm.x*std::cos(lift_)-arm.y*std::sin(lift_),
            arm.x*std::sin(lift_)+arm.y*std::cos(lift_),arm.z+bolt_);
    }
    bool holding_bolt() const { return holding_bolt_; }
    float bolt() const { return bolt_; }
    float lift() const { return lift_; }
    Magazine magazine() const { return magazine_; }
    int rounds() const { return rounds_; }

private:
    Magazine magazine_ = Magazine::Seated;
    int rounds_ = Capacity;
    unsigned ejections_ = 0;
    bool spent_case_ = false;
    float bolt_ = 0, lift_ = 0, grab_z_ = 0, grab_y_ = 0;
    bool grip_was_ = true, holding_bolt_ = false, opened_ = false, chambered_ = true;
};
}
