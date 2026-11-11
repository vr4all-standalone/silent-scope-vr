// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Semantic menu requests. No game memory, engine, renderer or input API is owned.
#include <cmath>
#include <cstdint>
namespace silent_scope { namespace dc_menu {
enum class MenuKind {None,Main,Training,Options,Ranking,Dialog,Pause,Continue,ModeSelect};
enum class MenuVerb {None,Focus,Confirm,Adjust,Back,Exit,Defaults};
struct MenuAction {
 MenuKind kind{MenuKind::None};MenuVerb verb{MenuVerb::None};
 int row{-1},column{-1},page{-1},direction{};
 std::uint64_t context_revision{};
 // Geometry's originating page, distinct from a requested destination page.
 int from_page{-1};
};
inline bool valid_action(const MenuAction& a)noexcept {
 return a.kind!=MenuKind::None&&a.verb!=MenuVerb::None&&a.context_revision!=0
  &&a.row>=-1&&a.row<128&&a.column>=-1&&a.column<128&&a.page>=-1&&a.page<128
  &&a.from_page>=-1&&a.from_page<128&&a.direction>=-1&&a.direction<=1
  &&(a.verb!=MenuVerb::Adjust||a.direction!=0);
}
struct MenuFacts {
 MenuKind kind{MenuKind::None};bool verified{},interactive{},context_active{true};
 std::uint64_t context_revision{};
 double native_seconds{-1};
 std::uint32_t source_context{}; // Source-attested sub-screen identity, not selection/page.
 int row{-1},column{-1},page{-1},row_count{},column_count{},page_count{};
 int value{},value_min{},value_max{};bool value_verified{};
 int previous_value{},next_value{};bool value_neighbors_verified{};
 int previous_page{-1},next_page{-1}; // Guarded native availability filter result.
 int mode_type{-1},variant{-1};double native_yaw{},native_pitch{},aim_gain_yaw{},aim_gain_pitch{};
 bool aim_verified{};std::uint32_t available_regions{};
 std::uint64_t aim_profile{};
 int native_wheel{};bool navigation_neutral{true};
 // Only a source-attested family may publish these; zero disables the verb.
 std::uint8_t confirm_button{},back_button{},exit_button{};
};
constexpr std::uint8_t NativeLeft=4,NativeRight=2,NativeMiddle=8;
inline constexpr std::uint32_t ArcadeModeContext=0x100;
inline constexpr std::uint32_t StoryRouteContext=0x101;
inline constexpr std::uint32_t InvasionRouteContext=0x104;
inline constexpr std::uint32_t OutdoorStageContext=0x106;
struct ModeGoal {bool valid{};double yaw{},pitch{};int row{-1},column{-1};};
inline int mode_region(double yaw,double pitch)noexcept {
 if(!std::isfinite(yaw)||!std::isfinite(pitch))return -1;
 // 0705c0 operates BEFORE the display cursor's piecewise pitch compression.
 if(pitch>2500)return 1;
 if(pitch> -2500)return 0;
 return yaw>1120?2:yaw> -1120?3:4;
}
inline int story_route_region(double yaw)noexcept {
 // The branch selector 070eda divides the three routes using raw yaw.
 if(!std::isfinite(yaw))return -1;
 return yaw>1950?0:yaw> -1790?1:2;
}
inline int mode_region_for_type(int type,double yaw,double pitch)noexcept {
 if(type==0)return mode_region(yaw,pitch);
 if(type==1)return story_route_region(yaw);
 if(type==4)return !std::isfinite(yaw)?-1:yaw>0?0:1;
 if(type!=6||!std::isfinite(yaw)||!std::isfinite(pitch))return -1;
 // Native Outdoor stage selector (072666), before display warping.
 const int column=yaw>=1568?0:yaw>=-1920?1:2;
 const int row=pitch>=0?0:pitch>=-1408?1:2;
 return row*3+column;
}
inline ModeGoal mode_goal(int type,int row,int column=-1)noexcept {
 if(type==6){
  if(row<0||row>=9||column!=-1)return {};
  constexpr double yaw[]{3200,0,-3350},pitch[]{672,-704,-2300};
  return {true,yaw[row%3],pitch[row/3],row,-1};
 }
 if(type==1){
  if(row<0||row>=3||column!=-1)return {};
  constexpr double centers[]{3000,0,-3000};
  return {true,centers[row],0,row,-1};
 }
 if(type==4){
  if(row<0||row>=2||column!=-1)return {};
  return {true,row==0?2550.:-2550.,0,row,-1};
 }
 if(type!=0||row<0||row>4)return {};
 ModeGoal g;g.valid=true;g.row=row;g.column=column;
 if(row==0){if(column!=-1)return {};}
 else if(row==1){if(column<0||column>1)return {};g.pitch=3500;g.yaw=column==0?1000:-1000;}
 else{if(column!=-1)return {};g.pitch=-3500;g.yaw=row==2?1800:row==3?0:-1800;}
 return g;
}
inline bool mode_position_ack(const MenuFacts& f,const ModeGoal& g)noexcept {
 const bool same_cell=mode_region_for_type(f.mode_type,f.native_yaw,f.native_pitch)==g.row
   &&(f.mode_type!=0||g.row!=1||f.column==g.column);
 return g.valid&&f.row==g.row&&same_cell
  // A relative mouse count moves by a discrete angle step. The closest
  // reachable position can be half a step from the nominal tile center.
  &&std::abs(f.native_yaw-g.yaw)<=2+std::abs(f.aim_gain_yaw)*.5
  &&std::abs(f.native_pitch-g.pitch)<=2+std::abs(f.aim_gain_pitch)*.5;
}
inline bool mode_goal_ready(const MenuFacts& f,const ModeGoal& g)noexcept {return f.aim_verified&&mode_position_ack(f,g);}
enum class PolicyStatus {Inactive,Invalid,Rearm,Idle,Moving,WaitingAck,Ready,Holding,AwaitPhase,Expired,Unsupported};
struct PolicyInput {
 double host_seconds{},native_seconds{};MenuFacts facts;
 MenuAction hover,clicked;bool fresh_click{},trigger_down{};
 double click_host_seconds{-1};std::uint8_t native_buttons{};
};
struct PolicyResult {
 int wheel{},mouse_x{};std::uint8_t buttons{};bool fresh_confirm{};
 PolicyStatus status{PolicyStatus::Inactive};MenuAction requested;
 ModeGoal aim_goal;
};
// One hardware pulse at a time, acknowledged by the actual native selection.
// Confirm is separately held for <=.1 guest seconds AND <=.25 host seconds.
// Main Menu intentionally stays with its physically accepted separate policy.
class Policy {
 enum class Axis {None,Row,Column,Page,Value};
 Axis flight_{Axis::None};int expected_{};double flight_host_{},flight_native_{};
 MenuKind kind_{MenuKind::None};std::uint64_t revision_{};bool release_required_{true};
 bool pending_{},await_phase_{};MenuAction goal_;double clicked_at_{};
 std::uint8_t hold_button_{};double hold_host_{},hold_native_{};
 static bool clock(double x){return std::isfinite(x)&&x>=0;}
 static bool supported(MenuKind k){return k!=MenuKind::None&&k!=MenuKind::Main;}
 static int current(const MenuFacts& f,Axis a){switch(a){case Axis::Row:return f.row;case Axis::Column:return f.column;case Axis::Page:return f.page;case Axis::Value:return f.value;default:return -1;}}
 bool bound(const MenuAction& a,const MenuFacts& f,bool permit_destination=false)const {
  if(!valid_action(a)||a.kind!=f.kind||a.context_revision!=f.context_revision)return false;
  if(f.kind==MenuKind::Options||f.kind==MenuKind::Ranking){
   if(a.from_page!=f.page&&!(permit_destination&&a.page==f.page))return false;
   if(a.verb!=MenuVerb::Focus&&a.page!=f.page)return false;
  }
  if(a.row>=0&&(f.kind==MenuKind::Options?a.row>f.row_count:a.row>=f.row_count))return false;
  if(a.column>=0&&a.column>=f.column_count)return false;
  if(a.page>=0&&(a.page>=f.page_count))return false;
  return true;
 }
 void cancel(){flight_=Axis::None;pending_=await_phase_=false;hold_button_=0;goal_={};}
 PolicyResult pulse(PolicyResult out,Axis axis,int next,int direction,const PolicyInput& i){
  flight_=axis;expected_=next;flight_host_=i.host_seconds;flight_native_=i.native_seconds;
  if(axis==Axis::Row)out.wheel=direction;else out.mouse_x=direction*16;
  out.status=PolicyStatus::Moving;return out;
 }
public:
 void reset()noexcept{*this=Policy{};}
 std::uint8_t duplicate_buttons(const PolicyInput& i){
  const auto& f=i.facts;
  if(!clock(i.host_seconds)||!clock(i.native_seconds)||!f.verified||!f.context_active||!f.interactive||!await_phase_
      ||kind_!=f.kind||revision_!=f.context_revision||!hold_button_||!f.navigation_neutral||f.native_wheel!=0
      ||(goal_.row>=0&&goal_.row!=f.row)||(goal_.column>=0&&goal_.column!=f.column)
      ||((f.kind==MenuKind::Options||f.kind==MenuKind::Ranking)&&goal_.page!=f.page)
      ||(f.kind==MenuKind::ModeSelect&&goal_.verb==MenuVerb::Confirm&&!mode_position_ack(f,mode_goal(f.mode_type,goal_.row,goal_.column)))
      ||i.host_seconds<hold_host_||i.native_seconds<hold_native_
      ||i.host_seconds-hold_host_>=.25||i.native_seconds-hold_native_>=.1){hold_button_=0;return 0;}
  return hold_button_;
 }
 PolicyResult poll(const PolicyInput& i){
  PolicyResult out;const auto& f=i.facts;
  if(!clock(i.host_seconds)||!clock(i.native_seconds)||!f.verified||!supported(f.kind)||!f.context_revision){
   cancel();release_required_=true;kind_=MenuKind::None;out.status=PolicyStatus::Invalid;return out;
  }
  if(kind_!=f.kind||revision_!=f.context_revision){cancel();release_required_=true;kind_=f.kind;revision_=f.context_revision;}
  if(!i.trigger_down)release_required_=false;
  if(!f.context_active){cancel();out.status=PolicyStatus::Inactive;return out;}
  if(release_required_){out.status=PolicyStatus::Rearm;return out;}
  if(!f.interactive){
   if((pending_&&i.host_seconds-clicked_at_>.5)||(flight_!=Axis::None&&(i.host_seconds-flight_host_>.3||i.native_seconds-flight_native_>.3))){
    cancel();release_required_=true;out.status=PolicyStatus::Expired;
   }else out.status=PolicyStatus::WaitingAck;
   return out;
  }
  // A delivered confirm locks navigation until a real native phase/context exit.
  // New clicks cannot race an unconsumed Left with a different row or wheel.
  if(await_phase_){
   out.requested=goal_;out.status=PolicyStatus::AwaitPhase;
   const bool same_row=goal_.row<0||goal_.row==f.row;
   const bool same_column=goal_.column<0||goal_.column==f.column;
   const bool same_page=(f.kind!=MenuKind::Options&&f.kind!=MenuKind::Ranking)||goal_.page==f.page;
   const bool same_mode=f.kind!=MenuKind::ModeSelect||goal_.verb!=MenuVerb::Confirm||mode_position_ack(f,mode_goal(f.mode_type,goal_.row,goal_.column));
   if(hold_button_&&same_row&&same_column&&same_page&&same_mode&&f.navigation_neutral&&f.native_wheel==0
       &&i.host_seconds-hold_host_<.25&&i.native_seconds-hold_native_<.1){
    out.buttons=hold_button_;out.status=PolicyStatus::Holding;
   }else hold_button_=0;
   return out;
  }
  if(i.fresh_click&&bound(i.clicked,f)&&clock(i.click_host_seconds)
      &&i.host_seconds-i.click_host_seconds>=-.02&&i.host_seconds-i.click_host_seconds<=.5){
   goal_=i.clicked;pending_=true;clicked_at_=i.click_host_seconds;
  }
  if(pending_&&i.host_seconds-clicked_at_>.5){pending_=false;out.status=PolicyStatus::Expired;return out;}
  const bool hover_page_arrow=i.hover.kind==MenuKind::Options&&i.hover.page!=i.hover.from_page;
  if(!pending_&&!hover_page_arrow&&bound(i.hover,f))goal_=i.hover;
  if(flight_!=Axis::None){
   if(current(f,flight_)==expected_&&f.navigation_neutral&&f.native_wheel==0){
    if(flight_==Axis::Page){goal_.from_page=f.page;if(goal_.verb==MenuVerb::Adjust)goal_.page=f.page;}
    flight_=Axis::None;
   }
   else if(i.host_seconds-flight_host_>.3||i.native_seconds-flight_native_>.3){
    cancel();release_required_=true;out.status=PolicyStatus::Expired;return out;
   }else{out.status=PolicyStatus::WaitingAck;return out;}
  }
  if(!bound(goal_,f)){goal_={};pending_=false;out.status=PolicyStatus::Idle;return out;}
  out.requested=goal_;
  const auto& a=goal_;
  if(f.kind==MenuKind::ModeSelect&&(a.verb==MenuVerb::Focus||a.verb==MenuVerb::Confirm)){
   out.aim_goal=mode_goal(f.mode_type,a.row,a.column);
   if(!out.aim_goal.valid||!f.aim_verified||!(f.available_regions&(1u<<unsigned(a.row)))){
    out.aim_goal={};pending_=false;out.status=PolicyStatus::Unsupported;return out;
   }
   if(!mode_goal_ready(f,out.aim_goal)){out.status=PolicyStatus::WaitingAck;return out;}
  }
  if(!f.navigation_neutral||f.native_wheel!=0||i.native_buttons){out.status=PolicyStatus::WaitingAck;return out;}
  if(f.kind==MenuKind::Options&&a.verb==MenuVerb::Focus&&a.page!=f.page){
   // Move to the native heading before changing a page; native arrows there
   // switch categories. At a value row the same X channel modifies a value.
   if(f.row>0)return pulse(out,Axis::Row,f.row-1,-1,i);
   const int forward=(a.page-f.page+f.page_count)%f.page_count;
   const int direction=forward<=f.page_count/2?1:-1;
   return pulse(out,Axis::Page,(f.page+direction+f.page_count)%f.page_count,direction,i);
  }
  if(f.kind!=MenuKind::ModeSelect){
   if(a.row>=0&&a.row!=f.row)return pulse(out,Axis::Row,f.row+(a.row>f.row?1:-1),a.row>f.row?1:-1,i);
   if(a.column>=0&&a.column!=f.column)return pulse(out,Axis::Column,f.column+(a.column>f.column?1:-1),a.column>f.column?1:-1,i);
  }
  if(a.verb==MenuVerb::Focus){pending_=false;out.status=PolicyStatus::Ready;return out;}
  if(!pending_){out.status=PolicyStatus::Idle;return out;}
  if(a.verb==MenuVerb::Adjust){
   pending_=false;
   if(f.kind==MenuKind::Options&&f.value_verified&&f.value_neighbors_verified&&f.row>0){
    const int value=a.direction<0?f.previous_value:f.next_value;
    if(value!=f.value&&value>=f.value_min&&value<=f.value_max)return pulse(out,Axis::Value,value,a.direction,i);
   }
   if(f.kind==MenuKind::Ranking){const int page=a.direction<0?f.previous_page:f.next_page;
    if(page>=0&&page<f.page_count&&page!=f.page)return pulse(out,Axis::Page,page,a.direction,i);}
   out.status=PolicyStatus::Unsupported;return out;
  }
  std::uint8_t button=0;
  if(a.verb==MenuVerb::Confirm)button=f.confirm_button;
  else if(a.verb==MenuVerb::Back)button=f.back_button;
  else if(a.verb==MenuVerb::Exit)button=f.exit_button;
  // Mouse has no proven L+R chord: Defaults remains explicitly unsupported.
  if(!button||(button&~(NativeLeft|NativeRight|NativeMiddle))){pending_=false;out.status=PolicyStatus::Unsupported;return out;}
  pending_=false;await_phase_=true;hold_button_=button;hold_host_=i.host_seconds;hold_native_=i.native_seconds;
  out.buttons=button;out.fresh_confirm=true;out.status=PolicyStatus::Holding;return out;
 }
};
}}
