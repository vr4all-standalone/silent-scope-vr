// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_view_runtime.h"
#include "dc_mouse_native.h"
#include "dc_mouse_input_source.h"
#include "cfg/option.h"
#include "emulator.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_cache.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "rend/vr_reproject.h"
#include <atomic>
#include <mutex>
#include <cstdio>

namespace silent_scope { namespace dc_view {
namespace {
struct NativeAck {
 bool limits{},wide{},camera_known{};float camera_factor{1};
 std::uint64_t epoch{},camera_revision{};
};
struct Runtime {
 std::mutex request_mutex,ack_mutex;
 RequestPolicy requests;NativeAck ack;
 std::atomic<bool> initialized{false},active{false},failure{false},render_shutdown{false};
 std::atomic<std::uint64_t> epoch{1};
 // Only the SH4 observer/VBlank path accesses the native policy/epoch/log count.
 NativeLimitsPolicy native;std::uint64_t native_epoch{};unsigned logs{};
};
Runtime& runtime(){static Runtime value;return value;}
bool title(){return settings.platform.isConsole()&&settings.content.gameId=="T9507N";}
void publish(bool enabled,bool wider){
 auto& s=runtime();const std::lock_guard<std::mutex> lock(s.request_mutex);
 if(s.requests.sync(enabled,wider,s.failure.load(std::memory_order_acquire)))vr::configureSilentScopeLens(s.requests.request());
}
void publish_current(){
 auto& s=runtime();publish(title()&&config::VrXr&&config::VrReproject
  &&s.active.load(std::memory_order_acquire)&&!s.render_shutdown.load(std::memory_order_acquire),config::VrWiden);
}
void clear_ack(){auto& s=runtime();const std::lock_guard<std::mutex> lock(s.ack_mutex);s.ack={};}
NativeAck copy_ack(){auto& s=runtime();const std::lock_guard<std::mutex> lock(s.ack_mutex);return s.ack;}
void synchronize_epoch(){
 auto& s=runtime();const auto epoch=s.epoch.load(std::memory_order_acquire);
 if(s.native_epoch!=epoch){s.native.new_epoch();s.native_epoch=epoch;}
}
LimitsResult service_limits(int target,bool recognized_main){
 auto& s=runtime();LimitsResult result;
 if(!title())return result;
 const auto* ram=RAM_SIZE==16u*1024*1024?GetMemPtr(0x0c000000u,RAM_SIZE):nullptr;
 if(ram==nullptr){
  result.failure=recognized_main||s.native.ready();
  if(result.failure)s.failure.store(true,std::memory_order_release);
  return result;
 }
 // Attest the same complete input code/literal contract as the native mouse
 // adapter before any write. Read-only stock OR wide detection handles loads.
 const auto stock=dc_mouse::read_source(ram,RAM_SIZE,settings.content.gameId);
 const auto wide_limits=dc_input::limits_for_factor(3);
 if(!stock.code_verified&&!dc_mouse::read_source(ram,RAM_SIZE,settings.content.gameId,wide_limits.yaw,wide_limits.pitch).code_verified){
  result.failure=recognized_main||s.native.ready();
  if(result.failure)s.failure.store(true,std::memory_order_release);
  return result;
 }
 bool wrote=false;
 try{
  result=s.native.synchronize(settings.content.gameId,target,recognized_main,
   [](std::uint32_t address){return ReadMem16_nommu(address);},
   [&](std::uint32_t address,std::uint16_t word){
    bm_RamWriteAccess(address);WriteMem16_nommu(address,word);wrote=true;
   },[&](dc_input::AimLimits limits){
    // Finish code invalidation before publishing the new native input caps.
    if(wrote){icache.Invalidate();wrote=false;}
    return dc_mouse::acknowledge_native_mouse_limits(limits.yaw,limits.pitch);
   });
 }catch(...){result={};result.failure=true;}
 if(wrote)icache.Invalidate(); // Includes a partial/failed write path.
 if(result.failure)s.failure.store(true,std::memory_order_release);
 return result;
}
void store_limits(const LimitsResult& result,std::uint64_t epoch){
 auto& s=runtime();const std::lock_guard<std::mutex> lock(s.ack_mutex);
 if(epoch!=s.epoch.load(std::memory_order_acquire)){s.ack={};return;}
 s.ack.limits=result.known&&result.acknowledged&&!result.failure;
 s.ack.wide=s.ack.limits&&result.wide;s.ack.epoch=epoch;
}
void observed_main(bool known,float factor,std::uint64_t revision){
 auto& s=runtime();
 if(!s.initialized.load(std::memory_order_acquire)||!dreamcast::ProductionCameraCertified||!title())return;
 synchronize_epoch();const auto epoch=s.native_epoch;
 const bool recognized=known&&(factor==1||factor==3);
 const auto request=vr::silentScopeLensRequestSnapshot();
 const bool wide=recognized&&factor==3&&request.enabled&&request.wide_proven
  &&s.active.load(std::memory_order_acquire)&&!s.failure.load(std::memory_order_acquire);
 const auto result=service_limits(wide?1:0,recognized);
 store_limits(result,epoch);
 {const std::lock_guard<std::mutex> lock(s.ack_mutex);
  if(epoch==s.epoch.load(std::memory_order_acquire)){
   s.ack.camera_known=recognized;s.ack.camera_factor=recognized?factor:1;s.ack.camera_revision=revision;
  }
 }
 if((result.changed||result.failure)&&s.logs<128){
  std::fprintf(stderr,"DC_VIEW_RUNTIME native epoch=%llu revision=%llu camera_known=%d factor=%.1f limits_known=%d wide=%d changed=%d acknowledged=%d failure=%d\n",
   static_cast<unsigned long long>(epoch),static_cast<unsigned long long>(revision),int(recognized),factor,
   int(result.known),int(result.wide),int(result.changed),int(result.acknowledged),int(result.failure));++s.logs;
 }
}
void vblank(){
 auto& s=runtime();
 if(!s.initialized.load(std::memory_order_acquire)||!dreamcast::ProductionCameraCertified||!title())return;
 const bool new_epoch=s.native_epoch!=s.epoch.load(std::memory_order_acquire);
 synchronize_epoch();
 const auto request=vr::silentScopeLensRequestSnapshot();
 const bool restore=!request.enabled||!s.active.load(std::memory_order_acquire)||s.failure.load(std::memory_order_acquire);
 // A pre-code startup probe is pending, never a failure. The first recognized
 // SDK main callback supplies readiness; a loaded exact state may resync now.
 if(!new_epoch&&!s.native.ready())return;
 const auto result=service_limits(restore?0:-1,false);
 store_limits(result,s.native_epoch);
}
void event(Event event,void*){
 auto& s=runtime();
 if(event==Event::VBlank){vblank();return;}
 if(event==Event::Terminate){
  s.active.store(false,std::memory_order_release);
  const auto old=vr::silentScopeLensRequestSnapshot();publish(false,old.wider);
 }else if(event==Event::Start||event==Event::LoadState)s.active.store(true,std::memory_order_release);
 s.epoch.fetch_add(1,std::memory_order_acq_rel);clear_ack();
}
void register_events(){
 static const bool registered=[](){
  for(auto e:{Event::Start,Event::LoadState,Event::DiskChange,Event::Terminate,Event::VBlank})EventManager::listen(e,event);
  return true;
 }();(void)registered;
}
}
void initialize_view_runtime(){
 register_events();auto& s=runtime();s.failure.store(false,std::memory_order_release);
 s.active.store(true,std::memory_order_release);s.render_shutdown.store(false,std::memory_order_release);
 s.epoch.fetch_add(1,std::memory_order_acq_rel);clear_ack();
 dreamcast::set_native_wider_observer(observed_main);
 s.initialized.store(true,std::memory_order_release);publish_current();
}
void update_view_runtime(const rend_context* context,float world_scale){
 auto& s=runtime();if(!s.initialized.load(std::memory_order_acquire))return;
 s.render_shutdown.store(false,std::memory_order_release);synchronize_view_intent();
 const auto request=vr::silentScopeLensRequestSnapshot();const auto ack=copy_ack();
 const bool acknowledged=ack.epoch==s.epoch.load(std::memory_order_acquire)&&ack.limits&&ack.wide
  &&ack.camera_known&&ack.camera_factor==3;
 const auto status=context?vr::silentScopeLensStatus(*context):dreamcast::LensStatus::Unavailable;
 dc_settings::set_runtime_capabilities(capabilities(request,s.failure.load(std::memory_order_acquire),acknowledged,status,world_scale));
}
void synchronize_view_intent(){
 if(runtime().initialized.load(std::memory_order_acquire))publish_current();
}
void shutdown_view_runtime(){
 auto& s=runtime();if(!s.initialized.load(std::memory_order_acquire))return;
 s.render_shutdown.store(true,std::memory_order_release);
 const auto request=vr::silentScopeLensRequestSnapshot();publish(false,request.wider);
 dc_settings::set_runtime_capabilities({});
}
}}
