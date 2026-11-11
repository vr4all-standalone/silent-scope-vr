// SPDX-License-Identifier: GPL-2.0-or-later
// Private single-control-thread native experiment, never a physical XR result.
#if defined(_WIN32) && !defined(LIBRETRO) && !defined(USE_OPENXR)
#include "dc_view_runtime_probe.h"
#include "dc_view_runtime.h"
#include "dc_mouse_native.h"
#include "dc_mouse_input_source.h"
#include "cfg/option.h"
#include "emulator.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "rend/vr_reproject.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace silent_scope { namespace dc_view_probe {
namespace {
bool flag(const char* key){const char* v=std::getenv(key);return v&&std::strcmp(v,"1")==0;}
struct Probe {
 std::atomic<std::uint64_t> epoch{1};std::uint64_t seen{};
 bool initialized{},blocked{},bound{},ended{};int last_intent{-1};unsigned logs{},last_phase{~0u};
 double last_log{-1},last_yaw{},last_pitch{};bool last_code{};int last_yaw_limit{},last_pitch_limit{};
};
Probe& state(){static Probe p;return p;}
bool title(){return settings.platform.isConsole()&&settings.content.gameId=="T9507N";}
double seconds(){return sh4_sched_now64()/double(SH4_MAIN_CLOCK);}
void sync_epoch(){auto& p=state();const auto epoch=p.epoch.load(std::memory_order_acquire);
 if(p.seen!=epoch){p.seen=epoch;p.last_intent=-1;p.ended=false;p.last_phase=~0u;
  if(p.bound)dc_xr::input_bridge().invalidate();
 }}
void event(Event e,void*){
 if(e!=Event::VBlank){state().epoch.fetch_add(1,std::memory_order_acq_rel);return;}
 auto& p=state();if(!p.initialized||p.blocked||!title())return;
 dc_view::synchronize_view_intent(); // Mirror render cadence, including failure->stock publication.
 sync_epoch();const auto r=recipe(seconds(),0);if(!r.owns)return;
 if(p.last_intent!=int(r.wider)){
  config::VrWiden.set(r.wider);dc_view::synchronize_view_intent();p.last_intent=int(r.wider);
  const auto request=vr::silentScopeLensRequestSnapshot();
  std::fprintf(stderr,"DC_VIEW_RUNTIME_PROBE intent synthetic_native=true xr=false native_seconds=%.9f wider=%d enabled=%d wide_proven=%d revision=%llu certified=%d\n",
   seconds(),int(request.wider),int(request.enabled),int(request.wide_proven),static_cast<unsigned long long>(request.revision),int(dreamcast::ProductionCameraCertified));
 }
}
}
bool environment_requested()noexcept{return flag("SILENT_SCOPE_DC_VIEW_RUNTIME_PROBE");}
void initialize_runtime_probe(){
 if(!environment_requested()||!title())return;
 auto& p=state();p.blocked=!dreamcast::ProductionCameraCertified||!config::VrXr||!config::VrReproject
  ||flag("SILENT_SCOPE_DC_MOUSE_CONSUMER_PROBE")||flag("SILENT_SCOPE_DC_MOUSE_CONSUMER_FIRE_PROBE");
 static const bool registered=[](){for(auto e:{Event::Start,Event::LoadState,Event::DiskChange,Event::Terminate,Event::VBlank})EventManager::listen(e,event);return true;}();(void)registered;
 p.epoch.fetch_add(1,std::memory_order_acq_rel);p.initialized=true;
 std::fprintf(stderr,"DC_VIEW_RUNTIME_PROBE initialized synthetic_native=true xr=false no_fire=true blocked=%d certified=%d config_xr=%d config_reproject=%d consumer_conflict=%d\n",
  int(p.blocked),int(dreamcast::ProductionCameraCertified),int(config::VrXr),int(config::VrReproject),int(flag("SILENT_SCOPE_DC_MOUSE_CONSUMER_PROBE")||flag("SILENT_SCOPE_DC_MOUSE_CONSUMER_FIRE_PROBE")));
}
bool poll_runtime_probe(std::uint8_t& buttons,int& x,int& y,int& wheel){
 if(!environment_requested()||!title()||seconds()<40)return false;
 auto& p=state();buttons=255;x=y=wheel=0;
 if(!p.initialized||p.blocked){if(p.logs++==0)std::fputs("DC_VIEW_RUNTIME_PROBE blocked=true input_neutral=true\n",stderr);return true;}
 sync_epoch();
 const double host=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(),native=seconds();
 auto r=recipe(native,host);dc_xr::Target emitted;bool owned=false;
 // Explicit private visibility replay: aim at an off-axis native actor without
 // changing saved camera RAM, the checkpoint, or any physical XR input.
 if(const char* value=std::getenv("SILENT_SCOPE_DC_SCOPE_VISIBILITY_AIM")){
  char* end=nullptr;const double yaw=std::strtod(value,&end);
  if(end!=value&&!*end&&std::isfinite(yaw)&&std::abs(yaw)<=5500){
   const double angle=yaw*6.2831853071795864769/65536.;
   r.target.native_direction={-std::sin(angle),0,-std::cos(angle)};
  }
 }
 if(r.drive){
  if(!p.bound){dc_xr::set_input_sink(dc_mouse::native_mouse_sink());p.bound=true;}
  dc_xr::input_bridge().submit(r.target,&emitted);owned=dc_mouse::poll_native_mouse(buttons,x,y,wheel);
  if(!owned)x=y=0;
 }else if(!p.ended){dc_xr::input_bridge().invalidate();dc_mouse::disable_native_mouse();p.ended=true;}
 buttons=255;wheel=0; // Every ray/request is no-fire; force neutral all buttons.
 const auto* ram=RAM_SIZE==16u*1024*1024?GetMemPtr(0x0c000000u,RAM_SIZE):nullptr;
 auto facts=dc_mouse::read_source(ram,RAM_SIZE,settings.content.gameId);
 const auto wide=dc_input::limits_for_factor(3);
 if(!facts.code_verified)facts=dc_mouse::read_source(ram,RAM_SIZE,settings.content.gameId,wide.yaw,wide.pitch);
 const auto hint=dc_mouse::source_hint(facts.context);
 const auto u16=[&](unsigned a){a&=0xffffff;return ram?unsigned(ram[a])|(unsigned(ram[a+1])<<8):0u;};
 const auto u32=[&](unsigned a){return u16(a)|(u16(a+2)<<16);};
 const auto request=vr::silentScopeLensRequestSnapshot();
 const bool changed=r.phase!=p.last_phase||x||y||facts.yaw!=p.last_yaw||facts.pitch!=p.last_pitch||facts.code_verified!=p.last_code
  ||facts.yaw_limit!=p.last_yaw_limit||facts.pitch_limit!=p.last_pitch_limit||native-p.last_log>=1;
 if(changed&&p.logs<511){
  std::fprintf(stderr,"DC_VIEW_RUNTIME_PROBE sample synthetic_native=true xr=false no_fire=true epoch=%llu native_seconds=%.9f host_seconds=%.9f phase=%u drive=%d wanted_wider=%d enabled=%d wide_proven=%d revision=%llu poll_owned=%d code=%d gain_verified=%d yaw=%.9g pitch=%.9g target_yaw=%.9g target_pitch=%.9g yaw_limit=%d pitch_limit=%d clamp_words=%04x,%04x,%04x,%04x delivered_x=%d delivered_y=%d delivered_buttons=0 live_sdk_focal=%08x live_mode=%u\n",
   static_cast<unsigned long long>(p.seen),native,host,r.phase,int(r.drive),int(request.wider),int(request.enabled),int(request.wide_proven),static_cast<unsigned long long>(request.revision),int(owned),int(facts.code_verified),int(hint.verified),facts.yaw,facts.pitch,emitted.yaw_units,emitted.pitch_units,
   facts.yaw_limit,facts.pitch_limit,u16(0x8c073ee4),u16(0x8c073ee6),u16(0x8c073ee8),u16(0x8c073eea),x,y,u32(0x8cd1f9c0),u32(0x8c1b0114));
  ++p.logs;p.last_log=native;
 }else if(changed&&p.logs==511){std::fputs("DC_VIEW_RUNTIME_PROBE log_limit_reached=true limit=512\n",stderr);++p.logs;}
 p.last_phase=r.phase;p.last_yaw=facts.yaw;p.last_pitch=facts.pitch;p.last_code=facts.code_verified;p.last_yaw_limit=facts.yaw_limit;p.last_pitch_limit=facts.pitch_limit;
 return true;
}
}}
#endif
