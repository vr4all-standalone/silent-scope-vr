// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_xr_bridge.h"
#include <optional>
#include <cmath>
#include <cstdlib>
#include <string_view>
namespace silent_scope { namespace dc_menu_typed_probe {
enum class Mode {Disabled,TrainingBack,TrainingStart,Options,Ranking};
inline Mode parse_mode(std::string_view mode){
 if(mode=="training-back")return Mode::TrainingBack;
 if(mode=="training-start")return Mode::TrainingStart;
 if(mode=="options")return Mode::Options;
 if(mode=="ranking")return Mode::Ranking;
 return Mode::Disabled;
}
inline Mode mode_from_environment(){const auto* text=std::getenv("SILENT_SCOPE_DC_TYPED_MENU_PROBE");return text?parse_mode(text):Mode::Disabled;}
enum class Stop {None,PhaseExit,Source,Context,Clock,Deadline,Unsupported};
// Private Windows-native experiment ONLY. The caller owns bridge submission and
// registration. This producer neither reads/writes RAM nor polls/fakes feedback.
// Once acquired, it retains ownership even when stopped: a boot producer must
// never resume synthetic clicks after a guarded experiment has terminated.
class Policy {
 Mode mode_;Stop stop_{Stop::None};bool owns_{},issued_{},released_{},final_{};
 dc_menu::MenuKind kind_{dc_menu::MenuKind::None};std::uint64_t revision_{};std::uint32_t source_context_{};
 int stage_{},expected_row_{-1},expected_column_{-1},expected_page_{-1},expected_value_{},base_value_{},ranking_first_{-1};
 bool expect_value_{};dc_menu::MenuAction captured_;
 double first_host_{},last_host_{-1},last_native_{-1},issued_host_{},issued_native_{},released_host_{},released_native_{};
 double ready_host_{-1},ready_native_{-1};
 static bool clock(double t){return std::isfinite(t)&&t>=0;}
 dc_menu::MenuKind wanted()const{
  if(mode_==Mode::TrainingBack||mode_==Mode::TrainingStart)return dc_menu::MenuKind::Training;
  if(mode_==Mode::Options)return dc_menu::MenuKind::Options;
  if(mode_==Mode::Ranking)return dc_menu::MenuKind::Ranking;
  return dc_menu::MenuKind::None;
 }
 dc_xr::Target neutral(double host,bool active=true)const{
  dc_xr::Target t;t.host_seconds=host;t.hand=1;t.native_ui=true;t.focused=t.tracked=active;
  t.ui_action.kind=kind_;t.ui_action.context_revision=revision_;return t;
 }
 dc_xr::Target halt(double host,Stop why){stop_=why;return neutral(host,false);}
 bool acknowledged(const dc_menu::MenuFacts& f)const{
  return (expected_row_<0||f.row==expected_row_)&&(expected_column_<0||f.column==expected_column_)
   &&(expected_page_<0||f.page==expected_page_)&&(!expect_value_||(f.value_verified&&f.value==expected_value_));
 }
 bool issue(double native,double host,const dc_menu::MenuFacts& f){
  using namespace dc_menu;
  captured_={};captured_.kind=kind_;captured_.context_revision=revision_;
  expected_row_=expected_column_=expected_page_=-1;expect_value_=false;final_=false;
  if(kind_==MenuKind::Training){
   captured_.row=expected_row_=3;captured_.column=expected_column_=2;
   captured_.verb=stage_==0?MenuVerb::Focus:(mode_==Mode::TrainingBack?MenuVerb::Back:MenuVerb::Confirm);
   final_=stage_==1;
   if(f.row_count!=4||f.column_count!=3)return false;
  }else if(kind_==MenuKind::Options){
   if(f.page_count!=6)return false;
   captured_.from_page=f.page;captured_.page=expected_page_=f.page;
   if(stage_<7){captured_.verb=MenuVerb::Focus;captured_.page=expected_page_=stage_%6;}
   else if(stage_==7){captured_.verb=MenuVerb::Focus;captured_.row=expected_row_=1;}
   else if(stage_==8||stage_==9){
    if(f.page!=0||f.row!=1||!f.value_verified||f.value_max<=f.value_min)return false;
    if(stage_==8){base_value_=f.value;if(base_value_>=f.value_max)return false;}
    else if(f.value!=base_value_+1||base_value_<f.value_min)return false;
    captured_.verb=MenuVerb::Adjust;captured_.row=expected_row_=1;captured_.direction=stage_==8?1:-1;
    expect_value_=true;expected_value_=stage_==8?base_value_+1:base_value_;
   }else {captured_.verb=MenuVerb::Exit;final_=true;}
  }else if(kind_==MenuKind::Ranking){
   captured_.from_page=captured_.page=f.page;
   if(stage_>=6||(stage_>0&&f.page==ranking_first_)){captured_.verb=MenuVerb::Back;final_=true;}
   else{
    if(f.page_count!=14||f.next_page<0||f.next_page>=14||f.next_page==f.page)return false;
    captured_.verb=MenuVerb::Adjust;captured_.direction=1;expected_page_=f.next_page;
   }
  }else return false;
  issued_=true;released_=false;issued_native_=native;issued_host_=host;ready_native_=ready_host_=-1;
  return valid_action(captured_);
 }
public:
 explicit Policy(Mode mode=Mode::Disabled):mode_(mode){}
 std::optional<dc_xr::Target> target(double native,double host,const dc_menu::MenuFacts& f){
  if(mode_==Mode::Disabled)return std::nullopt;
  if(!clock(native)||!clock(host)||(last_native_>=0&&(native<last_native_||host<last_host_))){
   if(!owns_)return std::nullopt;
   return halt(clock(host)?host:0,Stop::Clock);
  }
  last_native_=native;last_host_=host;
  if(stop_!=Stop::None)return neutral(host,false);
  if(!owns_){
   if(native<32)return std::nullopt;
   if(native>=75){owns_=true;kind_=wanted();revision_=f.context_revision;return halt(host,Stop::Deadline);}
   // Entry animation may already expose this family before its active phase.
   // Do not seize/cancel the experiment until native authority is active.
   if(!f.verified||f.kind!=wanted()||!f.context_revision||!f.context_active)return std::nullopt;
   owns_=true;kind_=f.kind;revision_=f.context_revision;source_context_=f.source_context;
   first_host_=host;released_host_=host;released_native_=native;ranking_first_=f.page;
  }
  if(native>=75||host-first_host_>=60)return halt(host,Stop::Deadline);
  if(!f.verified)return halt(host,Stop::Source);
  if(f.kind!=kind_||f.context_revision!=revision_||f.source_context!=source_context_)return halt(host,Stop::Context);
  if(!f.context_active)return halt(host,final_&&issued_?Stop::PhaseExit:Stop::Context);
  auto out=neutral(host);
  const bool stable=f.interactive&&f.navigation_neutral&&f.native_wheel==0;
  if(issued_){
   if(native-issued_native_>=3||host-issued_host_>=4)return halt(host,Stop::Deadline);
   if(!released_&&(native-issued_native_>=.12-1e-9||host-issued_host_>=.2-1e-9)){
    released_=true;released_native_=native;released_host_=host;
   }
   if(!released_){
    // Raw physical trigger is held independently of hit validity. The semantic
    // action was captured once, and no new rising edge is generated here.
    out.ui_select=true;out.ui_action=captured_;out.point_valid=stable;
    if(stable)out.native_point={0,0,-1};
    return out;
   }
   if(final_)return out;
   if(!stable||!acknowledged(f)){ready_native_=ready_host_=-1;return out;}
   if(ready_native_<0){ready_native_=native;ready_host_=host;}
   if(native-ready_native_<.2-1e-9||host-ready_host_<.2-1e-9||native-released_native_<.2-1e-9||host-released_host_<.2-1e-9)return out;
   ++stage_;issued_=false;
  }
  if(!stable){ready_native_=ready_host_=-1;return out;}
  if(native-released_native_<.2-1e-9||host-released_host_<.2-1e-9)return out;
  if(!issue(native,host,f))return halt(host,Stop::Unsupported);
  out.ui_select=out.point_valid=true;out.native_point={0,0,-1};out.ui_action=captured_;return out;
 }
 bool owns()const{return owns_;}bool idle()const{return !owns_;}
 bool completed()const{return stop_==Stop::PhaseExit;}
 bool cancelled()const{return stop_!=Stop::None&&stop_!=Stop::PhaseExit;}
 Stop stop_reason()const{return stop_;}int stage()const{return stage_;}
};
}}
