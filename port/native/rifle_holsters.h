// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rifle_support.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace silent_scope {
// A rear regrab is a new attachment, even when the fore-end remained held.
// Never turn a Trigger held during that transfer into a fresh gameplay shot.
class RiflePickupFireGate {
    bool rear_was_held_{},armed_{};
public:
    void reset(){rear_was_held_=armed_=false;}
    bool update(bool rear_held,bool pull){
        if(!rear_held||!rear_was_held_)armed_=false;
        rear_was_held_=rear_held;
        if(!pull)armed_=true;
        return rear_held&&armed_;
    }
};
// One equipped rifle. Both resting grips are in front of the chest: Arcade
// left, Simulation right. Mesh size only changes pickup reach/fore-end.
class RifleHolsters {
    std::array<bool,2> armed_{};
public:
    void reset(){armed_.fill(false);}
    static glm::mat4 pose(const glm::mat4& body,int slot){
        return glm::rotate(glm::translate(body,glm::vec3(slot==0?-.18f:.18f,-.40f,-.45f)),
            glm::radians(-90.f),glm::vec3(1,0,0));
    }
    int pick(const std::array<RifleSupportHand,2>& hands,const std::array<glm::mat4,2>& slots,
             const std::array<RifleForegrip,2>& fronts,const std::array<float,2>& sizes,
             int primary,bool occupied,bool enabled){
        if(!enabled||primary<0||primary>1){reset();return -1;}
        int choice=-1;float nearest=std::numeric_limits<float>::max();
        for(int h=0;h<2;++h){
            const auto& hand=hands[h];
            if(!hand.tracked||!hand.grip_valid){armed_[h]=false;continue;}
            if(!hand.grip){armed_[h]=true;continue;}
            if(occupied){armed_[h]=false;continue;}
            if(!armed_[h])continue;
            for(int slot=0;slot<2;++slot){
                const auto local=glm::vec3(glm::inverse(slots[slot])*hand.model[3]);
                const auto& grip=fronts[slot];
                const glm::vec3 contact=h==primary?glm::vec3(0):
                    glm::vec3(0,grip.height,std::clamp(local.z,grip.front,grip.rear));
                const float distance=glm::distance(local,contact);
                if(distance<RifleSupport::CaptureRadius*sizes[slot]&&distance<nearest){
                    nearest=distance;choice=slot;
                }
            }
        }
        if(choice>=0)for(int h=0;h<2;++h)if(hands[h].grip)armed_[h]=false;
        return choice;
    }
};
}
