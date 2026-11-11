// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_menu_actions.h"
#include <algorithm>
namespace silent_scope { namespace dc_menu {
struct AimPacket {int dx{},dy{};bool waiting{},fault{};};
// Isolated native-menu servo. It never accepts a world ray or gameplay gain.
// Relative deltas are delivered once, then acknowledged in RAW angle counters.
class AimServo {
 bool fault_{},flight_{},learned_{};int dx_{},dy_{};
 double yaw_{},pitch_{},host_{},native_{},gx_{},gy_{};
 double last_host_{-1},last_native_{-1};std::uint64_t last_sequence_{};
 static bool gain(double n){return std::isfinite(n)&&std::abs(n)>=.0001&&std::abs(n)<=128;}
 AimPacket fail(){fault_=true;flight_=false;return {0,0,false,true};}
public:
 void reset()noexcept{*this=AimServo{};}
 AimPacket poll(double host,double native,std::uint64_t sequence,const MenuFacts& f,const ModeGoal& goal){
  if(fault_)return {0,0,false,true};
  const bool valid_region=mode_region_for_type(f.mode_type,goal.yaw,goal.pitch)==goal.row;
  if(!goal.valid||!f.verified||!f.aim_verified||f.kind!=MenuKind::ModeSelect
      ||!std::isfinite(goal.yaw)||!std::isfinite(goal.pitch)||std::abs(goal.yaw)>5500||std::abs(goal.pitch)>4300
      ||goal.row<0||goal.row>=f.row_count||!valid_region
      ||!std::isfinite(host)||!std::isfinite(native)||host<0||native<0||host<last_host_||native<last_native_
      ||!std::isfinite(f.native_yaw)||!std::isfinite(f.native_pitch)||!gain(f.aim_gain_yaw)||!gain(f.aim_gain_pitch))return fail();
  last_host_=host;last_native_=native;
  if(sequence==last_sequence_)return {0,0,flight_,false};
  last_sequence_=sequence;
  if(flight_){
   const double x=f.native_yaw-yaw_,y=f.native_pitch-pitch_;
   if((dx_&&x*dx_*gx_<0)||(dy_&&y*dy_*gy_<0))return fail();
   const bool ax=!dx_||std::abs(x-dx_*gx_)<=2,ay=!dy_||std::abs(y-dy_*gy_)<=2;
   if(ax&&ay){
    if(dx_){const auto g=x/dx_;if(!gain(g))return fail();gx_=g;}
    if(dy_){const auto g=y/dy_;if(!gain(g))return fail();gy_=g;}
    learned_=true;flight_=false;
   }else if(host-host_>.25||native-native_>.25)return fail();
   else return {0,0,true,false};
  }
  if(!learned_){gx_=f.aim_gain_yaw;gy_=f.aim_gain_pitch;}
  const double ex=goal.yaw-f.native_yaw,ey=goal.pitch-f.native_pitch;
  const double cap=learned_?128.:16.;
  const auto count=[&](double error,double g){return std::abs(error)<=2?0:int(std::lround(std::clamp(error/g,-cap,cap)));};
  const int x=count(ex,gx_),y=count(ey,gy_);
  if(!x&&!y)return {};
  dx_=x;dy_=y;yaw_=f.native_yaw;pitch_=f.native_pitch;host_=host;native_=native;flight_=true;
  return {x,y,true,false};
 }
};
}}
