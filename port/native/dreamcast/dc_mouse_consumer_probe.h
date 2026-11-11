// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Private opt-in native Windows experiment. Synthetic tracking/focus and rays;
// this exercises production policy classes, not physical XR/controller input.
#include "dc_mouse_input_source.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace silent_scope { namespace dc_mouse_consumer_probe {
struct Recipe {bool owns{},active{},fire_mode{};unsigned phase{};dc_xr::Target target;};
inline Recipe recipe(double native_seconds,double host_seconds,bool fire_mode=false)noexcept{
 Recipe out;if(!std::isfinite(native_seconds)||!std::isfinite(host_seconds)||host_seconds<0||native_seconds<40)return out;
 out.owns=true;out.active=native_seconds<60;out.fire_mode=fire_mode;
 out.phase=native_seconds<42?0:native_seconds<44?1:native_seconds<46?2:native_seconds<48?3:
  native_seconds<50?4:native_seconds<52?5:native_seconds<54?6:native_seconds<56?7:native_seconds<60?8:9;
 const double yaw=out.phase==1?-40:out.phase==3?-120:0;
 const double pitch=out.phase==5?-40:out.phase==7?-120:0;
 constexpr double radians=6.2831853071795864769/65536.;
 const double a=yaw*radians,b=pitch*radians;
 auto& t=out.target;t.host_seconds=host_seconds;t.focused=t.tracked=t.direction_valid=true;t.hand=1;
 t.native_direction={-std::sin(a)*std::cos(b),-std::sin(b),-std::cos(a)*std::cos(b)};
 t.fire=fire_mode&&((native_seconds>=42.4&&native_seconds<42.5)
  ||(native_seconds>=44.4&&native_seconds<44.5)||(native_seconds>=46.4&&native_seconds<46.5));
 return out;
}
inline const char* status_name(dc_mouse::Status status)noexcept{
 using S=dc_mouse::Status;
 switch(status){case S::NoTarget:return "NoTarget";case S::InvalidContext:return "InvalidContext";
 case S::StaleTarget:return "StaleTarget";case S::Unreachable:return "Unreachable";
 case S::UnsupportedGain:return "UnsupportedGain";case S::WaitingAck:return "WaitingAck";
 case S::AckTimeout:return "AckTimeout";case S::Ready:return "Ready";case S::UiClickOnly:return "UiClickOnly";
 case S::DuplicatePoll:return "DuplicatePoll";case S::ExplicitStart:return "ExplicitStart";}
 return "Unknown";
}
struct Sample {
 dc_mouse::SourceFacts source;dc_mouse::Feedback feedback;dc_mouse::Packet packet;dc_xr::Target emitted;
 Recipe script;std::uint64_t generation{},poll{};bool reset{};int delivered_x{},delivered_y{};unsigned delivered_buttons{};
 unsigned guard_group{},guard_index{};double raw_yaw{},raw_pitch{};
};
// Metadata-only provenance: group1=layout,2=word,3=literal,4=clamp-code,
// 5=stock-limits,6=table,7=PDS pointer. Index addresses map to guarded arrays.
inline void inspect_guards(Sample& s,const std::uint8_t* ram,std::size_t bytes)noexcept{
 if(!ram||bytes!=16u*1024*1024){s.guard_group=1;return;}
 const auto r16=[&](std::uint32_t a){a&=0xffffff;return unsigned(ram[a])|(unsigned(ram[a+1])<<8);};
 const auto r32=[&](std::uint32_t a){return std::uint32_t(r16(a))|(std::uint32_t(r16(a+2))<<16);};
 const auto signed16=[&](std::uint32_t a){const int n=r16(a);return n<32768?n:n-65536;};
 s.raw_yaw=signed16(0x8c1b0862);s.raw_pitch=signed16(0x8c1b0860);
 unsigned i=0;for(auto g:dc_mouse::MouseWordGuards){if(r16(g.first)!=g.second){s.guard_group=2;s.guard_index=i;return;}++i;}
 i=0;for(auto g:dc_mouse::MouseLongGuards){if(r32(g.first)!=g.second){s.guard_group=3;s.guard_index=i;return;}++i;}
 for(i=0;i<dc_mouse::MouseClampGuards.size();++i)if(r16(0x8c073e7eu+2*i)!=dc_mouse::MouseClampGuards[i]){s.guard_group=4;s.guard_index=i;return;}
 const int limits[]{5500,-5500,4300,-4300};for(i=0;i<4;++i)if(signed16(0x8c073ee4u+2*i)!=limits[i]){s.guard_group=5;s.guard_index=i;return;}
 i=0;for(auto g:dc_mouse::MouseTableGuards){if(r32(g.first)!=g.second){s.guard_group=6;s.guard_index=i;return;}++i;}
 const auto ptr=r32(0x8c1d54ac),physical=ptr&0x1fffffff,area=ptr&0xe0000000u;
 if((area!=0&&area!=0x80000000u&&area!=0xa0000000u)||ptr%4||physical<0x0c000000||physical>0x0cffffdcu)s.guard_group=7;
}
class Runner {
 dc_mouse::Consumer consumer_;dc_xr::InputBridge bridge_;Sample last_;
 std::uint64_t generation_{},sequence_{};double last_native_{-1};bool have_{},ended_{},fire_mode_{};
public:
 Runner(){bridge_.set_sink(consumer_.sink());}
 const Sample& last()const noexcept{return last_;}
 bool poll(std::string_view title,double native_seconds,double host_seconds,const std::uint8_t* ram,std::size_t bytes,
  std::uint64_t generation,std::uint8_t& buttons,int& x,int& y,int& wheel,bool fire_mode=false){
  const auto script=recipe(native_seconds,host_seconds,fire_mode);
  if(title!="T9507N"||!script.owns)return false;
  Sample sample;sample.script=script;sample.generation=generation;sample.poll=++sequence_;
  if(!have_||generation!=generation_||native_seconds<last_native_||fire_mode!=fire_mode_){
   bridge_.invalidate();consumer_.reset(generation);ended_=false;sample.reset=true;
  }
  generation_=generation;last_native_=native_seconds;have_=true;fire_mode_=fire_mode;
  sample.source=dc_mouse::read_source(ram,bytes,title); // Stock caps only for this experiment.
  inspect_guards(sample,ram,bytes);
  auto& f=sample.feedback;f.code_verified=sample.source.code_verified;f.mouse_device=true;
  f.generation=generation;f.poll_sequence=sequence_;f.native_seconds=native_seconds;
  f.yaw=sample.source.yaw;f.pitch=sample.source.pitch;f.gain=dc_mouse::source_hint(sample.source.context);
  f.yaw_min=-double(sample.source.yaw_limit);f.yaw_max=sample.source.yaw_limit;
  f.pitch_min=-double(sample.source.pitch_limit);f.pitch_max=sample.source.pitch_limit;
  if(script.active){bridge_.submit(script.target,&sample.emitted);sample.packet=consumer_.poll(host_seconds,f);}
  else if(!ended_){bridge_.invalidate();consumer_.reset(generation);ended_=true;}
  // Default NO FIRE remains forced neutral even if policy regresses. Explicit
  // fire mode delivers only policy-acknowledged Left, never Right or Middle.
  sample.delivered_buttons=fire_mode&&script.active?(sample.packet.pressed&dc_mouse::Left):0;
  buttons=std::uint8_t(0xffu&~sample.delivered_buttons);x=sample.packet.dx;y=sample.packet.dy;wheel=0;
  sample.delivered_x=x;sample.delivered_y=y;last_=sample;return true;
 }
};
inline bool environment_enabled()noexcept{
 const char* value=std::getenv("SILENT_SCOPE_DC_MOUSE_CONSUMER_PROBE");return value&&std::strcmp(value,"1")==0;
}
inline bool environment_fire_enabled()noexcept{
 const char* value=std::getenv("SILENT_SCOPE_DC_MOUSE_CONSUMER_FIRE_PROBE");return value&&std::strcmp(value,"1")==0;
}
inline std::atomic<std::uint64_t>& environment_generation(){static std::atomic<std::uint64_t> value{1};return value;}
// Existing startup-registered native lifecycle callback calls this; no late
// EventManager listener registration or RAM access occurs in the callback.
inline void environment_invalidate()noexcept{environment_generation().fetch_add(1,std::memory_order_acq_rel);}
inline bool environment_poll(std::string_view title,double native_seconds,const std::uint8_t* ram,std::size_t bytes,
 std::uint8_t& buttons,int& x,int& y,int& wheel)noexcept{
 if(!environment_enabled()||title!="T9507N"||!std::isfinite(native_seconds)||native_seconds<40)return false;
 // Once the experiment owns native input, exceptions fail to neutral rather
 // than exposing the old scripted Middle60/Left62 sequence.
 buttons=0xff;x=y=wheel=0;
 static unsigned logs{};static bool failed{};
 try{
  if(failed)return true;
  static Runner runner;static Sample previous;static bool have_previous{};static double last_log{-1};
  const double host=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if(!runner.poll(title,native_seconds,host,ram,bytes,environment_generation().load(std::memory_order_acquire),buttons,x,y,wheel,environment_fire_enabled()))return true;
  const auto& s=runner.last();const auto& c=s.source.context;
  const bool change=!have_previous||s.reset||s.script.phase!=previous.script.phase||s.packet.status!=previous.packet.status
   ||s.packet.dx||s.packet.dy||s.feedback.yaw!=previous.feedback.yaw||s.feedback.pitch!=previous.feedback.pitch
   ||s.feedback.gain.signature!=previous.feedback.gain.signature||s.source.code_verified!=previous.source.code_verified
   ||s.source.buttons!=previous.source.buttons||s.emitted.fire!=previous.emitted.fire||s.packet.fresh_fire
   ||s.delivered_buttons!=previous.delivered_buttons||s.script.fire_mode!=previous.script.fire_mode
   ||native_seconds-last_log>=1;
  if(change&&logs<255){
   std::fprintf(stderr,"DC_MOUSE_CONSUMER schema=1 synthetic_native=true xr=false no_fire=%s probe_mode=%s generation=%llu poll=%llu native_seconds=%.9f phase=%u active=%d reset=%d code=%d source_code=%d mouse_override=%d digital=%d alt_a=%d alt_b=%d speed=%u reverse_x=%d reverse_y=%d mode=%u scope_branch=%u input_sign=%d pds=0x%08x buttons=0x%08x gain_verified=%d signature=%llu hint_yaw=%.9g hint_pitch=%.9g target_yaw=%.9g target_pitch=%.9g yaw=%.9g pitch=%.9g yaw_min=%.9g yaw_max=%.9g pitch_min=%.9g pitch_max=%.9g status=%s aligned=%d reachable=%d learned_yaw=%d learned_pitch=%d gain_yaw=%.9g gain_pitch=%.9g requested_x=%d requested_y=%d requested_buttons=%u delivered_x=%d delivered_y=%d delivered_buttons=%u guard_group=%u guard_index=%u raw_yaw=%.9g raw_pitch=%.9g target_fire=%d fresh_fire=%d ammo_source=unproven_use_geometry\n",
    s.script.fire_mode?"false":"true",s.script.fire_mode?"fire_probe":"no_fire_probe",static_cast<unsigned long long>(s.generation),static_cast<unsigned long long>(s.poll),native_seconds,s.script.phase,int(s.script.active),int(s.reset),
    int(s.source.code_verified),int(c.code_verified),int(c.mouse_override),int(c.digital_override),int(c.alternate_a),int(c.alternate_b),c.speed_index,int(c.reverse_x),int(c.reverse_y),c.mode,c.scope_branch,c.input_sign,s.source.peripheral_address,s.source.buttons,
    int(s.feedback.gain.verified),static_cast<unsigned long long>(s.feedback.gain.signature),s.feedback.gain.yaw,s.feedback.gain.pitch,s.emitted.yaw_units,s.emitted.pitch_units,
    s.feedback.yaw,s.feedback.pitch,s.feedback.yaw_min,s.feedback.yaw_max,s.feedback.pitch_min,s.feedback.pitch_max,status_name(s.packet.status),int(s.packet.aligned),int(s.packet.reachable),int(s.packet.learned_yaw),int(s.packet.learned_pitch),s.packet.yaw_gain,s.packet.pitch_gain,
    s.packet.dx,s.packet.dy,unsigned(s.packet.pressed),s.delivered_x,s.delivered_y,s.delivered_buttons,s.guard_group,s.guard_index,s.raw_yaw,s.raw_pitch,int(s.emitted.fire),int(s.packet.fresh_fire));
   ++logs;last_log=native_seconds;
  }else if(change&&logs==255){std::fputs("DC_MOUSE_CONSUMER log_limit_reached=true limit=256 further_samples_suppressed=true\n",stderr);++logs;}
  previous=s;have_previous=true;
 }catch(...){failed=true;if(logs<256){std::fputs("DC_MOUSE_CONSUMER resource_failure=true input_neutral=true\n",stderr);++logs;}}
 return true;
}
}}
