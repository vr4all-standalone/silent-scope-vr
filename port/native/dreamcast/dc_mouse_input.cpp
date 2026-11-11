// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_mouse_input.h"
#include "dc_menu_navigation.h"
#include "dc_menu_aim.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
namespace silent_scope { namespace dc_mouse {
GainHint source_hint(const SourceContext& c)noexcept {
 std::uint64_t signature=1469598103934665603ull;
 for(std::uint64_t value:{std::uint64_t(c.code_verified),std::uint64_t(c.mouse_override),std::uint64_t(c.digital_override),
     std::uint64_t(c.alternate_a),std::uint64_t(c.alternate_b),std::uint64_t(c.reverse_x),std::uint64_t(c.reverse_y),
     std::uint64_t(c.speed_index),std::uint64_t(c.mode),std::uint64_t(c.scope_branch),std::uint64_t(c.input_sign)})
     {signature^=value;signature*=1099511628211ull;}
 if(!c.code_verified||!c.mouse_override||c.digital_override||c.alternate_a||c.alternate_b
    ||c.speed_index>2||c.mode!=0||c.scope_branch!=1||c.input_sign>=0)return {false,signature,0,0};
 constexpr double speeds[]{.5,1,2};const double gain=40.*speeds[c.speed_index]/16.;
 return {true,signature,c.reverse_x?gain:-gain,c.reverse_y?gain:-gain};
}
namespace {
constexpr double StaleSeconds=.25,AckSeconds=.25,FireQueueSeconds=.25,StartSeconds=.1;
bool finite(double x){return std::isfinite(x);}
bool base_ok(const dc_xr::Target& t){return t.focused&&t.tracked&&!t.menu&&(t.hand==0||t.hand==1)&&finite(t.host_seconds)&&t.host_seconds>=0;}
bool point_ok(const dc_xr::Target& t){const auto& p=t.native_point;return t.point_valid&&finite(p.x)&&finite(p.y)&&finite(p.z)&&p.z<-.0001;}
bool aim_ok(const dc_xr::Target& t){const auto& d=t.native_direction;const double n=std::hypot(d.x,d.y,d.z);
 return finite(t.yaw_units)&&finite(t.pitch_units)&&(point_ok(t)||(t.direction_valid&&finite(n)&&std::abs(n-1)<.001));}
bool hint_ok(double gain){return finite(gain)&&std::abs(gain)>=.0001&&std::abs(gain)<=128;}
bool owns_menu_input(const dc_menu::MenuFacts& menu){
 return menu.verified&&menu.context_active&&menu.kind!=dc_menu::MenuKind::None&&menu.kind!=dc_menu::MenuKind::Main;
}
struct Edge {std::uint64_t id{};double at{-1},yaw{},pitch{};bool aim_valid{},main_menu{};int menu_item{-1};dc_menu::MenuAction menu_action;};
struct Snapshot {dc_xr::Target target{};bool have{};std::uint64_t reset{};Edge fire,ui,start,navigation;int navigation_direction{};};
struct Axis {double gain{},residual{};bool learned{};};
struct Flight {bool active{};int dx{},dy{};double yaw{},pitch{},last_yaw{},last_pitch{},host{},native{},gx{},gy{};bool trusted_x{},trusted_y{};};
}
struct Consumer::Impl {
 std::mutex mutex;Snapshot snapshot;dc_xr::Target previous;bool producer_have{},fire_arm{},ui_arm{},start_arm{},navigation_arm{};
 bool have_feedback{},have_metadata{},fault{};Status fault_status{Status::AckTimeout};
 Feedback previous_feedback;std::uint64_t seen_reset{},seen_fire{},seen_ui{},seen_start{},seen_navigation{};
 MenuNavigation navigation;
 MenuPointer pointer;
 dc_menu::Policy menu_actions;std::uint8_t menu_held{};
 dc_menu::AimServo menu_aim;
 double last_host{-1},pending_at{-1},middle_until{-1};bool hold_fire{};Axis x,y;Flight flight;
 double fault_host{-1},fault_native{-1};
 std::uint8_t held_native{};
 void clear_motion(){flight={};x={};y={};pending_at=-1;middle_until=-1;hold_fire=false;fault=false;held_native=0;navigation.reset();pointer.reset();menu_actions.reset();menu_aim.reset();menu_held=0;}
 void barrier(const Snapshot& s){clear_motion();seen_fire=s.fire.id;seen_ui=s.ui.id;seen_start=s.start.id;seen_navigation=s.navigation.id;}
 void cancel_aim(const Snapshot& s){const double until=middle_until;barrier(s);middle_until=until;}
 bool same_profile(const Feedback& a,const Feedback& b)const {
  const bool active_menu=owns_menu_input(a.menu);
  if(active_menu!=owns_menu_input(b.menu))return false;
  if(active_menu){
   return a.generation==b.generation&&a.menu.kind==b.menu.kind&&a.menu.context_revision==b.menu.context_revision
    &&a.menu.aim_profile==b.menu.aim_profile&&a.yaw_min==b.yaw_min&&a.yaw_max==b.yaw_max
    &&a.pitch_min==b.pitch_min&&a.pitch_max==b.pitch_max;
  }
  return a.generation==b.generation&&a.gain.signature==b.gain.signature&&a.gain.verified==b.gain.verified
   &&a.gain.yaw==b.gain.yaw&&a.gain.pitch==b.gain.pitch&&a.yaw_min==b.yaw_min&&a.yaw_max==b.yaw_max
   &&a.pitch_min==b.pitch_min&&a.pitch_max==b.pitch_max&&a.menu.kind==b.menu.kind&&a.menu.context_revision==b.menu.context_revision;
 }
 Packet consume(const Snapshot& s,double now,const Feedback& f) {
  Packet out;const auto& t=s.target;out.main_menu_context=t.native_ui&&t.ui_main_menu;out.menu_target=t.ui_pointer_item;
  const bool changed=s.reset!=seen_reset||!have_metadata||!same_profile(f,previous_feedback);
  if(changed){barrier(s);seen_reset=s.reset;}
  if(have_feedback&&(!finite(now)||now<last_host||!finite(f.native_seconds)||
      (!changed&&(f.native_seconds<previous_feedback.native_seconds||f.poll_sequence<previous_feedback.poll_sequence)))){
   barrier(s);out.status=Status::InvalidContext;return out;
  }
  last_host=now;
  if(!s.have){barrier(s);out.status=Status::NoTarget;return out;}
  if(!finite(now)||!base_ok(t)||!f.code_verified||!f.mouse_device||!finite(f.native_seconds)||f.native_seconds<0
     ||!finite(f.yaw)||!finite(f.pitch)||!finite(f.yaw_min)||!finite(f.yaw_max)||!finite(f.pitch_min)||!finite(f.pitch_max)
     ||f.yaw_min>=f.yaw_max||f.pitch_min>=f.pitch_max||f.yaw_min< -32768||f.yaw_max>32767||f.pitch_min< -32768||f.pitch_max>32767){barrier(s);out.status=Status::InvalidContext;return out;}
  if(now-t.host_seconds>StaleSeconds||t.host_seconds>now+.02){barrier(s);out.status=Status::StaleTarget;return out;}
  const bool duplicate=have_feedback&&!changed&&f.poll_sequence==previous_feedback.poll_sequence;
  previous_feedback=f;have_feedback=have_metadata=true;
  const bool known_menu=owns_menu_input(f.menu);
  if(duplicate){out.status=Status::DuplicatePoll;out.pressed=held_native;
   if(t.native_ui&&t.ui_main_menu&&(out.pressed&Left)&&!pointer.duplicate_left(now,f.native_seconds,
       f.main_menu_verified,f.menu_selected,f.menu_count,f.menu_native_wheel))out.pressed&=std::uint8_t(~Left);
   if(known_menu||(t.native_ui&&t.ui_action.kind!=dc_menu::MenuKind::None)){
    dc_menu::PolicyInput input;input.host_seconds=now;input.native_seconds=f.native_seconds;input.facts=f.menu;
    out.pressed=std::uint8_t((out.pressed&~menu_held)|menu_actions.duplicate_buttons(input));
   }
   return out;
  }
  if(!t.start)middle_until=-1;
  if(s.start.id>seen_start){seen_start=s.start.id;if(t.start&&now-s.start.at<=FireQueueSeconds)middle_until=f.native_seconds+StartSeconds;}
  if(t.start&&f.native_seconds<middle_until)out.pressed|=Middle;
  if(t.native_ui||known_menu){flight={};pending_at=-1;hold_fire=false;out.status=Status::UiClickOnly;out.ui_pointing_unverified=true;
   if(known_menu||(!t.ui_main_menu&&t.ui_action.kind!=dc_menu::MenuKind::None)){
    navigation.reset();pointer.reset();seen_navigation=s.navigation.id;
    if(fault){
     if(!t.ui_select&&!t.start&&!t.fire&&t.host_seconds>fault_host&&now>fault_host&&f.native_seconds>fault_native){barrier(s);out.pressed=0;return out;}
     out.pressed=0;out.menu_action_status=dc_menu::PolicyStatus::Unsupported;return out;
    }
    dc_menu::PolicyInput input;input.host_seconds=now;input.native_seconds=f.native_seconds;input.facts=f.menu;
    if(t.ui_action.verb==dc_menu::MenuVerb::Focus)input.hover=t.ui_action;
    input.fresh_click=s.ui.id>seen_ui;input.clicked=s.ui.menu_action;input.click_host_seconds=s.ui.at;
    input.trigger_down=t.ui_select;input.native_buttons=held_native;seen_ui=s.ui.id;
    const auto result=menu_actions.poll(input);menu_held=result.buttons;
    out.dx=result.mouse_x;out.wheel=result.wheel;out.menu_action_status=result.status;
    if(result.aim_goal.valid){
     if(held_native||out.pressed)return out;
     const auto aim=menu_aim.poll(now,f.native_seconds,f.poll_sequence,f.menu,result.aim_goal);
     if(aim.fault){fault=true;fault_status=Status::UnsupportedGain;fault_host=now;fault_native=f.native_seconds;
      menu_held=0;out.dx=out.dy=out.wheel=0;out.pressed=0;out.menu_action_status=dc_menu::PolicyStatus::Unsupported;return out;}
     out.dx=aim.dx;out.dy=aim.dy;
     if(!aim.waiting)out.pressed|=result.buttons;
    }else{menu_aim.reset();out.pressed|=result.buttons;}
    return out;
   }
   menu_actions.reset();menu_held=0;
   const bool fresh=s.navigation.id>seen_navigation;seen_navigation=s.navigation.id;
   if(f.menu_navigation_verified)out.wheel=navigation.poll(now,t.ui_navigation,
      fresh&&now-s.navigation.at<=FireQueueSeconds,s.navigation_direction);
   else navigation.reset();
   const bool fresh_click=s.ui.id>seen_ui;seen_ui=s.ui.id;
   if(t.ui_main_menu){
    MenuPointerInput input;input.host_seconds=now;input.native_seconds=f.native_seconds;
    input.source_verified=f.main_menu_verified;input.selected=f.menu_selected;input.count=f.menu_count;input.native_wheel=f.menu_native_wheel;
    input.hovered=t.ui_pointer_item;input.trigger_down=t.ui_select;input.native_left_down=(held_native&Left)!=0;
    input.fresh_click=fresh_click&&s.ui.main_menu;input.clicked=s.ui.menu_item;input.click_host_seconds=s.ui.at;input.manual_wheel=out.wheel;
    const auto result=pointer.poll(input);out.wheel=result.wheel;out.menu_target=result.requested_item;out.menu_pointer_status=result.status;
    if(result.left_down)out.pressed|=Left;
   }else{
    pointer.reset();
    if(fresh_click&&t.ui_select&&point_ok(t)&&now-s.ui.at<=FireQueueSeconds)out.pressed|=Left;
   }
   return out;
  }
  if(!aim_ok(t)||f.yaw<f.yaw_min||f.yaw>f.yaw_max||f.pitch<f.pitch_min||f.pitch>f.pitch_max){
   cancel_aim(s);out.pressed&=Middle;out.status=out.pressed?Status::ExplicitStart:Status::InvalidContext;return out;}
  out.reachable=t.yaw_units>=f.yaw_min&&t.yaw_units<=f.yaw_max&&t.pitch_units>=f.pitch_min&&t.pitch_units<=f.pitch_max;
  if(!out.reachable){cancel_aim(s);out.status=Status::Unreachable;out.pressed&=Middle;return out;}
  if(!f.gain.verified||!hint_ok(f.gain.yaw)||!hint_ok(f.gain.pitch)){
   cancel_aim(s);out.status=Status::UnsupportedGain;out.pressed&=Middle;return out;
  }
  if(!t.fire)hold_fire=false;
  if(s.fire.id>seen_fire){seen_fire=s.fire.id;
   if(now-s.fire.at<=FireQueueSeconds&&s.fire.aim_valid&&s.fire.yaw>=f.yaw_min&&s.fire.yaw<=f.yaw_max
      &&s.fire.pitch>=f.pitch_min&&s.fire.pitch<=f.pitch_max)pending_at=s.fire.at;
  }
  if(pending_at>=0&&now-pending_at>FireQueueSeconds)pending_at=-1;
  if(fault){
   if(!t.fire&&!t.ui_select&&!t.start&&t.host_seconds>fault_host&&now>fault_host&&f.native_seconds>fault_native){
    barrier(s);out.pressed=0;out.status=Status::Ready;return out; // Fresh release + valid source feedback, then require a new edge.
   }
   out.status=fault_status;out.pressed&=Middle;return out;
  }
  if(flight.active){
   const double dx=f.yaw-flight.yaw,dy=f.pitch-flight.pitch;
   const bool moved_x=!flight.dx||std::abs(dx)>=.5,moved_y=!flight.dy||std::abs(dy)>=.5;
   const bool wrong=(flight.dx&&dx*flight.dx*f.gain.yaw<0)||(flight.dy&&dy*flight.dy*f.gain.pitch<0);
   if(wrong){fault=true;fault_status=Status::UnsupportedGain;fault_host=now;fault_native=f.native_seconds;
    pending_at=-1;hold_fire=false;flight={};out.status=fault_status;return out;}
   const bool stable=f.yaw==flight.last_yaw&&f.pitch==flight.last_pitch;
   const bool trusted=(!flight.dx||(flight.trusted_x&&std::abs(dx-flight.dx*flight.gx)<=std::max(1.,std::abs(dx)*.05)))
       &&(!flight.dy||(flight.trusted_y&&std::abs(dy-flight.dy*flight.gy)<=std::max(1.,std::abs(dy)*.05)));
   if(moved_x&&moved_y&&(stable||trusted)){
    if(flight.dx&&f.yaw>f.yaw_min&&f.yaw<f.yaw_max){const double gain=dx/flight.dx;
     if(!hint_ok(gain)){fault=true;fault_status=Status::UnsupportedGain;}else{x.gain=gain;x.learned=true;}}
    if(flight.dy&&f.pitch>f.pitch_min&&f.pitch<f.pitch_max){const double gain=dy/flight.dy;
     if(!hint_ok(gain)){fault=true;fault_status=Status::UnsupportedGain;}else{y.gain=gain;y.learned=true;}}
    flight.active=false;
   }else if(now-flight.host>AckSeconds||f.native_seconds-flight.native>AckSeconds){
    fault=true;fault_status=Status::AckTimeout;pending_at=-1;hold_fire=false;flight={};
   }else{flight.last_yaw=f.yaw;flight.last_pitch=f.pitch;}
  }
  if(fault){fault_host=now;fault_native=f.native_seconds;out.status=fault_status;out.pressed&=Middle;return out;}
  out.learned_yaw=x.learned;out.learned_pitch=y.learned;out.yaw_gain=x.learned?x.gain:f.gain.yaw;out.pitch_gain=y.learned?y.gain:f.gain.pitch;
  const double error_x=t.yaw_units-f.yaw,error_y=t.pitch_units-f.pitch;
  const double tolerance_x=std::max(2.,std::abs(out.yaw_gain)*.6),tolerance_y=std::max(2.,std::abs(out.pitch_gain)*.6);
  out.aligned=std::abs(error_x)<=tolerance_x&&std::abs(error_y)<=tolerance_y;
  if(!flight.active&&!out.aligned){
   const auto count=[](double error,double tolerance,Axis& a,double hint){
    if(std::abs(error)<=tolerance){a.residual=0;return 0;}
    const double gain=a.learned?a.gain:hint;
    const double requested=error/gain+a.residual;
    const double lo=a.learned?-512.:-16.,hi=a.learned?511.:16.;
    const double bounded=std::clamp(requested,lo,hi);const int n=int(std::lround(bounded));
    a.residual=requested==bounded?std::clamp(bounded-n,-.5,.5):0.;return n;
   };
   out.dx=count(error_x,tolerance_x,x,f.gain.yaw);out.dy=count(error_y,tolerance_y,y,f.gain.pitch);
   if(out.dx||out.dy)flight={true,out.dx,out.dy,f.yaw,f.pitch,f.yaw,f.pitch,now,f.native_seconds,out.yaw_gain,out.pitch_gain,x.learned,y.learned};
  }
  out.status=flight.active?Status::WaitingAck:Status::Ready;
  // Maple receives the actual native mouse button and relative motion. A
  // moving XR goal need not exactly equal older native feedback to pull the
  // trigger. All code/device/gain/bounds/fault/lifecycle gates above still apply.
  // Preserve a fresh tap released between native polls for one unique packet.
  if(pending_at>=0){
   // A physical release/repress may also fit between native polls. Deliver Up
   // first so fresh_fire always describes an actual native Left transition.
   if(held_native&Left){hold_fire=false;return out;}
   hold_fire=t.fire;pending_at=-1;out.fresh_fire=true;out.pressed|=Left;
  }
  if(hold_fire&&t.fire)out.pressed|=Left;
  return out;
 }
};
Consumer::Consumer():impl_(new Impl){}Consumer::~Consumer()=default;
void Consumer::submit(const dc_xr::Target& t){
 auto& p=*impl_;const std::lock_guard<std::mutex> lock(p.mutex);
 const bool transition=!p.producer_have||p.previous.epoch!=t.epoch||p.previous.hand!=t.hand||p.previous.focused!=t.focused
  ||p.previous.tracked!=t.tracked||p.previous.menu!=t.menu||p.previous.native_ui!=t.native_ui||p.previous.ui_main_menu!=t.ui_main_menu
  ||p.previous.ui_action.kind!=t.ui_action.kind||p.previous.ui_action.context_revision!=t.ui_action.context_revision||!finite(t.host_seconds)
  ||(p.producer_have&&t.host_seconds<p.previous.host_seconds);
 if(transition){++p.snapshot.reset;p.fire_arm=p.ui_arm=p.start_arm=p.navigation_arm=false;}
 const bool enabled=base_ok(t);
 const auto edge=[&](bool button,bool& armed,Edge& e){if(!enabled)armed=false;
  else if(!button)armed=true;else if(armed){++e.id;e.at=t.host_seconds;e.yaw=t.yaw_units;e.pitch=t.pitch_units;
   e.aim_valid=aim_ok(t)&&!t.native_ui;e.main_menu=t.ui_main_menu;
   e.menu_item=t.ui_main_menu&&point_ok(t)&&t.ui_pointer_item>=0&&t.ui_pointer_item<4?t.ui_pointer_item:-1;
   e.menu_action=t.native_ui&&point_ok(t)?t.ui_action:dc_menu::MenuAction{};armed=false;}};
 edge(t.fire,p.fire_arm,p.snapshot.fire);edge(t.ui_select,p.ui_arm,p.snapshot.ui);edge(t.start,p.start_arm,p.snapshot.start);
 if(!enabled||!t.native_ui||t.ui_navigation< -1||t.ui_navigation>1)p.navigation_arm=false;
 else if(!t.ui_navigation)p.navigation_arm=true;
 else if(p.navigation_arm&&(!p.producer_have||p.previous.ui_navigation!=t.ui_navigation)){
  ++p.snapshot.navigation.id;p.snapshot.navigation.at=t.host_seconds;p.snapshot.navigation_direction=t.ui_navigation;
 }
 p.snapshot.target=t;p.snapshot.have=true;p.previous=t;p.producer_have=true;
}
void Consumer::reset(std::uint64_t){auto& p=*impl_;const std::lock_guard<std::mutex> lock(p.mutex);
 ++p.snapshot.reset;p.snapshot.have=false;p.producer_have=false;p.fire_arm=p.ui_arm=p.start_arm=p.navigation_arm=false;}
dc_xr::InputSink Consumer::sink()noexcept{return {this,[](void* p,const dc_xr::Target& t){static_cast<Consumer*>(p)->submit(t);},
 [](void* p,std::uint64_t e){static_cast<Consumer*>(p)->reset(e);}};}
Packet Consumer::poll(double now,const Feedback& f){Snapshot copy;{const std::lock_guard<std::mutex> lock(impl_->mutex);copy=impl_->snapshot;}
 auto packet=impl_->consume(copy,now,f);
 {const std::lock_guard<std::mutex> lock(impl_->mutex);if(impl_->snapshot.reset!=copy.reset){impl_->barrier(impl_->snapshot);packet={};packet.status=Status::InvalidContext;}}
 impl_->held_native=packet.pressed;return packet;}
}}
