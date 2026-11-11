// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
namespace silent_scope {
// Both poses must use the same world coordinates. The primary model already
// includes the accepted gun-angle calibration; its origin is the grip position.
struct RifleSupportHand {glm::mat4 model{1};bool tracked{},grip{},grip_valid{true};};
struct RifleForegrip {float height{-.025f},front{-.49f},rear{-.18f};};
struct RifleSupportContext {bool focused{true},menu{};int active_hand{1};RifleForegrip foregrip{};bool restore_support{};float size{1.f};};
// Use this single model for the visible rifle, scope and native -Z aim.
struct RifleSupportResult {glm::mat4 model{1};bool supported{},valid{};};
// Neutral-safe Schmitt latch. Reset/start reports held until an actual <=.35
// sample: a mid-range squeeze after interruption must not arm support as released.
// Forward valid() to RifleSupportHand::grip_valid: held alone is not validity.
class GripLatch {
 bool held_{true},neutral_required_{true},valid_{};
public:
 static constexpr float Engage=.65f,Release=.35f;
 bool valid()const{return valid_;}
 bool engaged()const{return valid_&&!neutral_required_&&held_;}
 void reset(){held_=neutral_required_=true;valid_=false;}
 bool update(float value){
  if(!std::isfinite(value)||value<0||value>1){reset();return true;}
  valid_=true;
  if(neutral_required_){if(value<=Release){neutral_required_=false;held_=false;}return held_;}
  if(held_){if(value<=Release)held_=false;}else if(value>=Engage)held_=true;
  return held_;
 }
};
class RifleSupport {
 int hand_{-1};bool armed_{},latched_{};glm::vec3 ray0_{};glm::quat q0_{1,0,0,0};
 void cancel(){armed_=latched_=false;}
 static bool finite(glm::vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
 static bool valid_pose(const glm::mat4& m){
  for(int i=0;i<4;++i)for(int j=0;j<4;++j)if(!std::isfinite(m[i][j]))return false;
  if(std::abs(m[3][3]-1)>1e-4f)return false;
  for(int i=0;i<3;++i){
   if(std::abs(m[i][3])>1e-4f||std::abs(glm::dot(glm::vec3(m[i]),glm::vec3(m[i]))-1)>1e-3f)return false;
   for(int j=i+1;j<3;++j)if(std::abs(glm::dot(glm::vec3(m[i]),glm::vec3(m[j])))>1e-3f)return false;
  }
  return glm::determinant(glm::mat3(m))>.999f;
 }
 static float foregrip_distance(glm::vec3 local,RifleForegrip grip){
  const glm::vec3 nearest{0,grip.height,std::clamp(local.z,grip.front,grip.rear)};
  return glm::length(local-nearest);
 }
 glm::quat swing(glm::vec3 ray)const{
  const float alignment=std::clamp(glm::dot(ray0_,ray),-1.f,1.f);
  if(alignment<-.999999f){
   // At an exact half-turn the shortest axis is ambiguous. Use captured up,
   // projected perpendicular to the hand line, rather than normalizing zero.
   auto axis=q0_*glm::vec3(0,1,0);axis-=ray0_*glm::dot(axis,ray0_);
   if(glm::dot(axis,axis)<1e-8f){axis=q0_*glm::vec3(1,0,0);axis-=ray0_*glm::dot(axis,ray0_);}
   return glm::angleAxis(glm::pi<float>(),glm::normalize(axis));
  }
  const auto cross=glm::cross(ray0_,ray);
  return glm::normalize(glm::quat(1+alignment,cross.x,cross.y,cross.z));
 }
public:
 // Procedural rifle_mesh.h fore-end occupies z=-.49..-.18 and y=-.025...025.
 static constexpr glm::vec3 foregrip_anchor{0,-.025f,-.34f};
 static constexpr float CaptureRadius=.12f,ReleaseRadius=.20f;
 void reset(){hand_=-1;cancel();}
 RifleSupportResult update(const std::array<RifleSupportHand,2>& hands,RifleSupportContext context){
  if(context.active_hand!=hand_){cancel();hand_=context.active_hand;}
  if(hand_<0||hand_>1){cancel();return {};}
  const auto& primary=hands[hand_];const auto& offhand=hands[1-hand_];
  if(!primary.tracked||!valid_pose(primary.model)){cancel();return {};}
  RifleSupportResult result{primary.model,false,true};
  if(!context.focused||context.menu||!offhand.tracked||!offhand.grip_valid||!valid_pose(offhand.model)){cancel();return result;}
  if(!offhand.grip){latched_=false;armed_=true;return result;}
  const glm::vec3 origin(primary.model[3]),off(offhand.model[3]);
  const auto delta=off-origin;const float distance=glm::length(delta);
  if(!finite(delta)||!std::isfinite(distance)||distance<.12f*context.size||distance>1.2f*context.size){cancel();return result;}
  const auto ray=delta/distance;const auto rotation=glm::mat3(primary.model);
  if(context.restore_support){
   ray0_=ray;q0_=glm::normalize(glm::quat_cast(rotation));
   armed_=false;latched_=true;
   result.supported=true;
   return result;
  }
  if(!latched_){
   if(!armed_)return result;
   if(foregrip_distance(glm::transpose(rotation)*delta,context.foregrip)>CaptureRadius*context.size)return result;
   // Keep waiting while a valid armed squeeze approaches the actual fore-end.
   // Capture the complete accepted pose so attachment itself does not snap.
   armed_=false;ray0_=ray;q0_=glm::normalize(glm::quat_cast(rotation));
   latched_=true;
  }
  const auto base=glm::normalize(swing(ray)*q0_);
  const auto primary_q=glm::normalize(glm::quat_cast(rotation));
  const auto expected=base*glm::vec3(0,0,-1),primary_forward=primary_q*glm::vec3(0,0,-1);
  const float alignment=std::clamp(glm::dot(expected,primary_forward),-1.f,1.f);
  // Keep the60-degree plausibility/behind guard, measuring barrel swing rather
  // than the off-axis contact ray: pure wrist twist must not detach support.
  if(!finite(expected)||!std::isfinite(alignment)||alignment<.5f){cancel();return result;}
  const auto relative=glm::normalize(glm::conjugate(base)*primary_q);
  const float twist_length=std::hypot(relative.w,relative.z);
  const auto twist=twist_length>1e-6f?glm::quat(relative.w/twist_length,0,0,relative.z/twist_length):glm::quat(1,0,0,0);
  const auto solved=glm::mat3_cast(glm::normalize(base*twist));
  // Deliberate sniper tradeoff: positions determine forward; primary local-Z
  // twist supplies roll. This avoids roll-driven aim tilt, but does not enforce
  // a perfectly rigid off-axis point contact. The expanded exit capsule bounds it.
  for(int i=0;i<3;++i)result.model[i]=glm::vec4(solved[i],0);
  // Translation is copied untouched: rotation is about the active grip.
  if(!valid_pose(result.model)){cancel();return {primary.model,false,true};}
  if(foregrip_distance(glm::transpose(solved)*delta,context.foregrip)>ReleaseRadius*context.size){cancel();return {primary.model,false,true};}
  result.supported=true;return result;
 }
};
}
