// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace silent_scope { namespace dc_mouse {
enum class MenuPointerStatus {Inactive,Ready,WheelSent,WaitingAck,WaitingWheelNeutral,Confirm,HoldingLeft,AwaitingPhase,ReleaseBeforeConfirm,Expired,Fault,InvalidSource};
inline const char* menu_pointer_status_name(MenuPointerStatus status)noexcept{
 switch(status){
 case MenuPointerStatus::Inactive:return "inactive";case MenuPointerStatus::Ready:return "ready";
 case MenuPointerStatus::WheelSent:return "wheel_sent";case MenuPointerStatus::WaitingAck:return "waiting_ack";
 case MenuPointerStatus::WaitingWheelNeutral:return "wheel_drain";case MenuPointerStatus::Confirm:return "left_packet";
 case MenuPointerStatus::HoldingLeft:return "left_hold";
 case MenuPointerStatus::AwaitingPhase:return "awaiting_phase";case MenuPointerStatus::ReleaseBeforeConfirm:return "release_before_left";
 case MenuPointerStatus::Expired:return "expired";case MenuPointerStatus::Fault:return "fault";
 case MenuPointerStatus::InvalidSource:return "invalid_source";
 }return "invalid";
}
struct MenuPointerInput {
 // fresh_click is the Consumer's release-rearmed, current-epoch mailbox edge,
 // not a second edge detector based on the latest native-polled held level.
 double host_seconds{},native_seconds{},click_host_seconds{};bool source_verified{},trigger_down{},fresh_click{},native_left_down{};
 int selected{-1},count{},native_wheel{},hovered{-1},clicked{-1},manual_wheel{};
};
struct MenuPointerResult {int wheel{},requested_item{-1};bool confirm{},left_down{};MenuPointerStatus status{MenuPointerStatus::Ready};};
class MenuPointer {
 bool flight_{},fault_{},await_phase_{},retry_pending_{},hold_{};int from_{},expected_{},clicked_{-1},confirmed_{-1};
 double sent_host_{},sent_native_{},click_at_{},retry_at_{},hold_native_until_{},hold_host_until_{},last_host_{-1},last_native_{-1};
 static bool item(int n){return n>=0&&n<4;}
 void begin_hold(const MenuPointerInput& s,MenuPointerResult& out){
  hold_=true;hold_native_until_=s.native_seconds+.1;hold_host_until_=s.host_seconds+.25;
  out.confirm=out.left_down=true;out.status=MenuPointerStatus::Confirm;
 }
public:
 void reset()noexcept{*this=MenuPointer{};}
 // Duplicate native polls may retain a bounded Left level, but must never
 // advance a wheel command or consume a new physical mailbox edge.
 bool duplicate_left(double host,double native,bool source,int selected,int count,int wheel)noexcept{
  if(!source||count!=4||selected!=confirmed_||wheel||!std::isfinite(host)||!std::isfinite(native)){
   hold_=false;retry_pending_=false;return false;
  }
  if(host>=hold_host_until_||native>=hold_native_until_)hold_=false;
  return hold_;
 }
 MenuPointerResult poll(const MenuPointerInput& s)noexcept{
  MenuPointerResult out;
  if(!s.source_verified||s.count!=4||!item(s.selected)||s.hovered< -1||s.hovered>3||s.clicked< -1||s.clicked>3
     ||s.manual_wheel< -1||s.manual_wheel>1||!std::isfinite(s.host_seconds)||!std::isfinite(s.native_seconds)
     ||s.host_seconds<0||s.native_seconds<0||(last_host_>=0&&s.host_seconds<last_host_)||(last_native_>=0&&s.native_seconds<last_native_)){
   reset();out.status=MenuPointerStatus::InvalidSource;return out;
  }
  last_host_=s.host_seconds;last_native_=s.native_seconds;
  if(fault_){
   if(!s.trigger_down&&s.hovered==-1&&!s.fresh_click)reset();
   else{out.status=MenuPointerStatus::Fault;return out;}
  }
  if(await_phase_){
   out.requested_item=confirmed_;
   // No later intent may move the highlight under an older Left whose guest
   // consumption is not yet proven. A fresh same-item retry cannot move it.
   if(s.fresh_click&&s.clicked==confirmed_&&s.selected==confirmed_&&s.native_wheel==0
      &&std::isfinite(s.click_host_seconds)&&s.host_seconds-s.click_host_seconds>=-.02&&s.host_seconds-s.click_host_seconds<=.5){
    retry_pending_=true;retry_at_=s.click_host_seconds;
   }
   else if(s.fresh_click)retry_pending_=false;
   if(retry_pending_&&(s.host_seconds-retry_at_>.5||s.selected!=confirmed_||s.native_wheel!=0))retry_pending_=false;
   if(s.selected!=confirmed_||s.native_wheel!=0){hold_=false;retry_pending_=false;out.status=MenuPointerStatus::AwaitingPhase;return out;}
   if(hold_){
    if(s.native_seconds<hold_native_until_&&s.host_seconds<hold_host_until_){out.left_down=true;out.status=MenuPointerStatus::HoldingLeft;return out;}
    hold_=false;out.status=retry_pending_?MenuPointerStatus::ReleaseBeforeConfirm:MenuPointerStatus::AwaitingPhase;return out;
   }
   if(retry_pending_){
    if(s.native_left_down){out.status=MenuPointerStatus::ReleaseBeforeConfirm;return out;}
    retry_pending_=false;begin_hold(s,out);return out;
   }
   out.status=MenuPointerStatus::AwaitingPhase;return out;
  }
  if(s.fresh_click){
   clicked_=-1;
   if(item(s.clicked)&&std::isfinite(s.click_host_seconds)&&s.host_seconds-s.click_host_seconds>=-.02
       &&s.host_seconds-s.click_host_seconds<=.5){clicked_=s.clicked;click_at_=s.click_host_seconds;await_phase_=false;}
  }
  if(clicked_>=0&&s.host_seconds-click_at_>.5){clicked_=-1;out.status=MenuPointerStatus::Expired;}
  if(flight_){
   if((s.selected!=from_&&s.selected!=expected_)||s.host_seconds-sent_host_>.25||s.native_seconds-sent_native_>.25){
    flight_=false;clicked_=-1;fault_=true;out.status=MenuPointerStatus::Fault;return out;
   }
   if(s.selected==expected_&&s.native_wheel==0)flight_=false;
   else{out.requested_item=clicked_>=0?clicked_:s.hovered;
    out.status=s.selected==expected_?MenuPointerStatus::WaitingWheelNeutral:MenuPointerStatus::WaitingAck;return out;}
  }
  if(s.native_wheel){out.status=MenuPointerStatus::WaitingWheelNeutral;return out;}
  const int goal=clicked_>=0?clicked_:(item(s.hovered)?s.hovered:
      (s.manual_wheel?std::clamp(s.selected+s.manual_wheel,0,3):-1));
  out.requested_item=goal;
  if(goal<0)return out;
  if(goal!=s.selected){
   out.wheel=goal>s.selected?1:-1;from_=s.selected;expected_=s.selected+out.wheel;flight_=true;
   sent_host_=s.host_seconds;sent_native_=s.native_seconds;out.status=MenuPointerStatus::WheelSent;return out;
  }
  if(clicked_>=0){
   if(s.native_left_down){out.status=MenuPointerStatus::ReleaseBeforeConfirm;return out;}
   confirmed_=clicked_;clicked_=-1;await_phase_=true;begin_hold(s,out);
  }
  return out;
 }
};
}}
