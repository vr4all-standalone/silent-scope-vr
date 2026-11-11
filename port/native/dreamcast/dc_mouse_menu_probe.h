// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Explicit Windows native diagnostic only. Hardware Maple responses and bounded
// readback metadata; no native RAM writes, device changes or automatic game fire.
#include "dc_mouse_menu_source.h"
#include "dc_mouse_input.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>
namespace silent_scope { namespace dc_menu_probe {
struct Sample {bool owns{};std::uint8_t pressed{};int wheel{};double elapsed{};dc_mouse::MenuSourceFacts facts;};
class Policy {
 bool started_{},cancelled_{};double first_{},last_{};unsigned sent_{};
public:
 Sample poll(std::string_view title,double native,const std::uint8_t* ram,std::size_t bytes)noexcept{
  if(title!="T9507N"||!std::isfinite(native)||native<0)return {};
  if(!started_){first_=last_=native;started_=true;}
  if(native<last_)cancelled_=true;
  last_=native;Sample out;out.owns=true;out.elapsed=native-first_;
  out.facts=dc_mouse::read_menu_source(ram,bytes,title);
  if(cancelled_||out.elapsed>=40)return out;
  if(out.elapsed<15&&out.elapsed<std::floor(out.elapsed)+.1)out.pressed=dc_mouse::Left;
  if(out.elapsed>=20&&out.elapsed<20.2)out.pressed=dc_mouse::Middle;
  constexpr double at[]{23,25,27,29};constexpr int wheel[]{1,1,-1,-1};
  for(unsigned i=0;i<4;++i){
   if(!(sent_&(1u<<i))&&out.elapsed>=at[i]){
    sent_|=1u<<i;
    if(out.elapsed<at[i]+.2&&out.facts.code_verified&&out.facts.mouse_present)out.wheel=wheel[i];
   }
  }
  return out;
 }
};
inline bool environment_poll(std::string_view title,double native,const std::uint8_t* ram,std::size_t bytes,
                             std::uint8_t& buttons,int& x,int& y,int& wheel)noexcept{
 static const bool enabled=[](){const auto* p=std::getenv("SILENT_SCOPE_DC_MENU_NAV_PROBE");return p&&std::string_view(p)=="1";}();
 if(!enabled)return false;
 static Policy policy;static unsigned records{};static double logged{-1};static int selection{-999};
 const auto out=policy.poll(title,native,ram,bytes);if(!out.owns)return false;
 buttons=std::uint8_t(0xffu&~out.pressed);x=y=0;wheel=out.wheel;
 if(records<96&&(out.wheel||out.facts.selection!=selection||native-logged>=1)){
  std::fprintf(stderr,"DC_MENU_NAV_PROBE synthetic_native=true xr=false native_seconds=%.9f elapsed=%.9f code=%d mouse=%d state=%u selection=%d count=%d native_wheel=%d delivered_wheel=%d buttons=%u\n",
   native,out.elapsed,out.facts.code_verified,out.facts.mouse_present,out.facts.main_menu_state,
   out.facts.selection,out.facts.count,out.facts.wheel,out.wheel,unsigned(out.pressed));
  ++records;logged=native;selection=out.facts.selection;
 }
 return true;
}
}}
