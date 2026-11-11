// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
namespace silent_scope {
struct RifleSmoothingContext {double seconds{};int hand{1},level{1};bool supported{},available{true};};
struct RifleSmoothingResult {
    glm::mat4 model{1};bool valid{};
    // True only when a valid sample seeds new history: startup or a lifecycle
    // boundary. Invalid/unavailable samples return valid=false; the next valid
    // sample reports the restart once. Off does not report a restart each frame.
    bool restarted{};
};
class RifleSmoothing {
    bool have_{};RifleSmoothingContext previous_;
    glm::quat filtered_{1,0,0,0};
    static bool valid(const glm::mat4& pose){
        for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(pose[c][r]))return false;
        if(std::abs(pose[3][3]-1)>1e-4f)return false;
        for(int c=0;c<3;++c){
            if(std::abs(pose[c][3])>1e-4f||std::abs(glm::dot(glm::vec3(pose[c]),glm::vec3(pose[c]))-1)>1e-3f)return false;
            for(int r=c+1;r<3;++r)if(std::abs(glm::dot(glm::vec3(pose[c]),glm::vec3(pose[r])))>1e-3f)return false;
        }
        return glm::determinant(glm::mat3(pose))>.999f;
    }
    static float alpha(double seconds,double dt){
        // Exact exponential response for the elapsed time, including uneven
        // XR frame intervals. No velocity boost may bypass the chosen weight.
        return float(-std::expm1(-dt/seconds));
    }
public:
    void reset(){have_=false;}
    RifleSmoothingResult update(const glm::mat4& model,RifleSmoothingContext context){
        if(!context.available||context.hand<0||context.hand>1||!std::isfinite(context.seconds)||context.seconds<0||!valid(model)){
            reset();return {};
        }
        if(context.level<0||context.level>3)context.level=1;
        const auto current=glm::normalize(glm::quat_cast(glm::mat3(model)));
        const double dt=context.seconds-previous_.seconds;
        const bool restart=!have_||context.hand!=previous_.hand||context.level!=previous_.level||
            context.supported!=previous_.supported||dt<=0||dt>.25;
        if(restart||context.level==0)filtered_=current;
        else {
            // Deliberate angular weight: Low 60ms, Medium 120ms, High 240ms.
            // Grip translation remains raw and all rifle/optic/aim consumers
            // receive the same rotation. Choose the shortest quaternion arc.
            constexpr double response_seconds[]{0,.06,.12,.24};
            const auto target=glm::dot(filtered_,current)<0?-current:current;
            filtered_=glm::normalize(glm::slerp(filtered_,target,alpha(response_seconds[context.level],dt)));
        }
        previous_=context;have_=true;
        if(context.level==0)return {model,true,restart};
        auto result=glm::mat4_cast(filtered_);result[3]=model[3];
        return {result,true,restart};
    }
};
}
