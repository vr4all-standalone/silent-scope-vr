// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_xr_bridge.h"
#include <cmath>
namespace silent_scope { namespace dc_xr {
void InputBridge::set_sink(InputSink sink){
 if(sink.user==sink_.user&&sink.submit==sink_.submit&&sink.reset==sink_.reset)return;
 invalidate();sink_=sink;have_previous_=false;previous_seconds_=-1;invalidate();
}
void InputBridge::invalidate(){armed_=navigation_armed_=false;++epoch_;if(sink_.reset)sink_.reset(sink_.user,epoch_);}
bool InputBridge::submit(Target target,Target* emitted){
 const bool clock=std::isfinite(target.host_seconds)&&target.host_seconds>=0;
 if(!have_previous_||focused_!=target.focused||tracked_!=target.tracked||menu_!=target.menu||native_ui_!=target.native_ui||main_menu_!=target.ui_main_menu||hand_!=target.hand||menu_kind_!=target.ui_action.kind||menu_revision_!=target.ui_action.context_revision||
    !clock||target.host_seconds<previous_seconds_)invalidate();
 have_previous_=true;focused_=target.focused;tracked_=target.tracked;menu_=target.menu;native_ui_=target.native_ui;main_menu_=target.ui_main_menu;hand_=target.hand;
 previous_seconds_=clock?target.host_seconds:-1;
 menu_kind_=target.ui_action.kind;menu_revision_=target.ui_action.context_revision;
 const bool enabled=clock&&target.focused&&target.tracked&&!target.menu&&(target.hand==0||target.hand==1);
 if(!enabled||!target.native_ui||target.ui_navigation< -1||target.ui_navigation>1)navigation_armed_=false;
 else if(!target.ui_navigation)navigation_armed_=true;
 if(!enabled||!target.native_ui||!navigation_armed_)target.ui_navigation=0;
 if(!enabled)armed_=false;
 const bool fresh=armed_;
 if(enabled&&!target.fire&&!target.ui_select&&!target.start)armed_=true;
 const auto& p=target.native_point;
 target.point_valid=target.point_valid&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.z<-.0001;
 const auto& d=target.native_direction;
 const double length=std::hypot(d.x,d.y,d.z);
 target.direction_valid=target.direction_valid&&std::isfinite(d.x)&&std::isfinite(d.y)&&std::isfinite(d.z)&&
     std::isfinite(length)&&std::abs(length-1)<.001;
 const bool native_aim=target.point_valid||target.direction_valid;
 if(native_aim&&!target.native_ui){
  const auto& aim=target.point_valid?p:d;
  constexpr double units=65536./6.2831853071795864769;
  target.yaw_units=-std::atan2(aim.x,-aim.z)*units;
  // Renderer coordinates have +Y down. Native pitch increases upward: native
  // +32 mouse Y moved the verified pitch counter to -80.
  target.pitch_units=-std::atan2(aim.y,std::hypot(aim.x,aim.z))*units;
 }else{if(!target.point_valid)target.native_point={};target.yaw_units=target.pitch_units=0;}
 if(!target.point_valid)target.native_point={};
 if(!target.direction_valid)target.native_direction={};
 target.ui_main_menu=enabled&&target.native_ui&&target.ui_main_menu;
 if(!target.ui_main_menu||!target.point_valid||target.ui_pointer_item<0||target.ui_pointer_item>3)target.ui_pointer_item=-1;
 target.fire=enabled&&fresh&&native_aim&&!target.native_ui&&target.fire;
 // Main Menu preserves raw physical trigger level even off-panel, so moving a
 // held trigger into an item cannot invent a fresh click. Hit validity is
 // captured separately by the consumer at the physical edge.
 target.ui_select=enabled&&fresh&&(target.point_valid||target.ui_main_menu||target.ui_action.kind!=dc_menu::MenuKind::None)&&target.native_ui&&target.ui_select;
 target.start=enabled&&fresh&&target.start;
 target.epoch=epoch_;
 if(!sink_.submit){target.fire=target.ui_select=target.start=false;target.ui_navigation=0;if(emitted)*emitted=target;return false;}
 if(emitted)*emitted=target;
 sink_.submit(sink_.user,target);return true;
}
InputBridge& input_bridge(){static InputBridge bridge;return bridge;}
void set_input_sink(InputSink sink){input_bridge().set_sink(sink);}
void FrameRates::reset(){*this=FrameRates{};}
void FrameRates::sample(double now,uint64_t generation,uint64_t sequence){
 if(!std::isfinite(now)||now<0){reset();return;}
 if(!have_||now<last_||generation!=generation_){
  reset();have_=true;since_=last_=now;generation_=generation;sequence_=sequence;return;
 }
 ++xr_;if(sequence&&sequence!=sequence_){++native_;sequence_=sequence;}
 last_=now;const double elapsed=now-since_;
 if(elapsed>=1){xr_hz_=xr_/elapsed;native_hz_=native_/elapsed;since_=now;xr_=native_=0;}
}
}}
