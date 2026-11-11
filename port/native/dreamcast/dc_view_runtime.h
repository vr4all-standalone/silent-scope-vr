// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_aim_limits.h"
#include "dc_wider_control.h"
#include "dc_settings.h"
#include <cmath>
struct rend_context;
namespace silent_scope { namespace dc_view {
class RequestPolicy {
 dreamcast::LensRequest request_;bool published_{};
public:
 bool sync(bool enabled,bool wider,bool failure,bool certified=dreamcast::ProductionCameraCertified)noexcept{
  const bool proven=certified&&!failure;
  if(published_&&request_.enabled==enabled&&request_.wider==wider&&request_.wide_proven==proven)return false;
  request_.enabled=enabled;request_.wider=wider;request_.wide_proven=proven;++request_.revision;published_=true;return true;
 }
 dreamcast::LensRequest request()const noexcept{return request_;}
};
inline dc_settings::RuntimeCapabilities capabilities(const dreamcast::LensRequest& request,bool failure,
 bool acknowledged_wide,dreamcast::LensStatus actual,double world_scale,
 bool certified=dreamcast::ProductionCameraCertified)noexcept{
 dc_settings::RuntimeCapabilities out;out.wider_available=certified&&!failure;
 out.wider_applied=out.wider_available&&request.enabled&&request.wider&&request.wide_proven
  &&acknowledged_wide&&actual==dreamcast::LensStatus::Wider;
 if(std::isfinite(world_scale)&&world_scale>0){const double units=1/world_scale;if(std::isfinite(units))out.game_units_per_metre=units;}
 return out;
}
struct LimitsResult {bool known{},wide{},changed{},acknowledged{},failure{};};
// SH4-thread policy. No direct engine calls; read/write/ack form audited seams.
// Target=-1 adopts the exact existing state,0 restores stock,1 applies wide.
class NativeLimitsPolicy {
 dc_input::AimLimitPatch patch_;bool ready_{},needs_ack_{true};
public:
 void new_epoch()noexcept{patch_.forget();ready_=false;needs_ack_=true;}
 bool ready()const noexcept{return ready_;}
 template<class Read,class Write,class Ack>LimitsResult synchronize(std::string_view title,int target,
  bool recognized_main,Read read,Write write,Ack ack){
  LimitsResult out;
  if(!ready_){if(!patch_.adopt(title,read)){out.failure=recognized_main;return out;}ready_=true;}
  if(!patch_.guard(title,read)){ready_=false;needs_ack_=true;out.failure=true;return out;}
  const bool before=patch_.wider(),wide=target<0?before:target!=0;
  if(!patch_.apply(title,wide,read,write)||!patch_.guard(title,read)){
   ready_=false;needs_ack_=true;out.failure=true;return out;
  }
  out.known=true;out.wide=wide;out.changed=before!=wide;
  if(needs_ack_||out.changed){
   if(!ack(dc_input::limits_for_factor(wide?3:1))){needs_ack_=true;out.failure=true;return out;}
   needs_ack_=false;
  }
  out.acknowledged=true;return out;
 }
};
// Startup only, after native mouse initialization and before emulation/events.
void initialize_view_runtime();
// Host/config only. Normal callers are startup/render; an explicitly isolated
// nonXR diagnostic may call on its sole control thread. No RAM or caps update.
void synchronize_view_intent();
// Render thread only: config/owned-context/host acknowledgements, no RAM access.
void update_view_runtime(const rend_context* context,float world_scale);
// Render publishes disabled intent; guarded restoration occurs on SH4 boundary.
void shutdown_view_runtime();
}}
