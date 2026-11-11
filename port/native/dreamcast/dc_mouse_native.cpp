// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_mouse_native.h"
#include "dc_mouse_input_source.h"
#include "dc_mouse_menu_source.h"
#include "dc_menu_source.h"
#include "cfg/option.h"
#include "emulator.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "input/mouse.h"
#include "log/Log.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <tuple>
#if defined(_WIN32) && !defined(LIBRETRO) && !defined(USE_OPENXR)
#include "dc_menu_pointer_probe.h"
#endif

namespace silent_scope { namespace dc_mouse {
namespace {
static_assert((1u<<Mouse::LEFT_BUTTON)==Left,"Native Left changed");
static_assert((1u<<Mouse::RIGHT_BUTTON)==Right,"Native Right changed");
static_assert((1u<<Mouse::MIDDLE_BUTTON)==Middle,"Native Middle changed");
constexpr std::size_t RamBytes=16u*1024*1024;
constexpr std::uint64_t StockLimits=(std::uint64_t(5500)<<32)|4300;
struct NativeState {
 Consumer consumer;
 std::mutex activation;
 std::atomic<bool> initialized{false},enabled{false};
 std::atomic<std::uint64_t> generation{1},limits{StockLimits};
 std::uint64_t sequence{}; // Only the Maple emulation thread reads/writes.
 std::mutex menu_mutex;dc_menu::MenuFacts menu_snapshot;
 std::uint64_t menu_revision{1};
};
NativeState& state(){static NativeState value;return value;}
bool matches_title(){return settings.platform.isConsole()&&settings.content.gameId=="T9507N"
 &&RAM_SIZE==RamBytes&&config::MapleMainDevices[0]==MDT_Mouse;}
void invalidate(Event,void*){
 auto& s=state();s.limits.store(StockLimits,std::memory_order_release);
 const auto generation=s.generation.fetch_add(1,std::memory_order_acq_rel)+1;
 s.consumer.reset(generation);
 {const std::lock_guard<std::mutex> lock(s.menu_mutex);s.menu_snapshot={};s.menu_snapshot.context_revision=++s.menu_revision;}
}
void register_events(){
 static const bool once=[](){
  for(auto event:{Event::Start,Event::LoadState,Event::DiskChange,Event::Terminate})
   EventManager::listen(event,invalidate);
  return true;
 }();
 (void)once;
}
double host_seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void log_menu(const Packet& packet,const Feedback& feedback,const MenuSourceFacts& menu){
 struct Key {bool active{},source{};std::uint64_t generation{};unsigned state{};std::uint32_t table{};
  int selected{},count{},target{},wheel{},native_wheel{};unsigned left{},consumer{};MenuPointerStatus policy{};
  bool operator==(const Key& o)const{return std::tie(active,source,generation,state,table,selected,count,target,wheel,native_wheel,left,consumer,policy)
    ==std::tie(o.active,o.source,o.generation,o.state,o.table,o.selected,o.count,o.target,o.wheel,o.native_wheel,o.left,o.consumer,o.policy);}
  bool operator!=(const Key& o)const{return !(*this==o);}
 };
 static unsigned records{};static Key previous{};
 Key current{packet.main_menu_context,feedback.main_menu_verified,feedback.generation,menu.main_menu_state,menu.table_pointer,
  menu.selection,menu.count,packet.menu_target,packet.wheel,menu.wheel,unsigned((packet.pressed&Left)!=0),unsigned(packet.status),packet.menu_pointer_status};
 if(!current.active){if(previous.active&&records<96){NOTICE_LOG(INPUT,"SS_MENU phase_exit gen=%llu",static_cast<unsigned long long>(feedback.generation));++records;}previous=current;return;}
 if(records<96&&current!=previous){
  NOTICE_LOG(INPUT,"SS_MENU native_s=%.6f gen=%llu state=%u table=%08x source=%u selected=%d/%d target=%d policy=%s consumer=%u wheel=%d native_wheel=%d Left=%u",
   feedback.native_seconds,static_cast<unsigned long long>(feedback.generation),menu.main_menu_state,unsigned(menu.table_pointer),unsigned(feedback.main_menu_verified),
   menu.selection,menu.count,packet.menu_target,menu_pointer_status_name(packet.menu_pointer_status),unsigned(packet.status),packet.wheel,menu.wheel,current.left);
  ++records;
 }
 previous=current;
}
}

void initialize_native_mouse(){
 register_events();auto& s=state();const std::lock_guard<std::mutex> lock(s.activation);
 s.enabled.store(false,std::memory_order_release);
 invalidate(Event::Start,nullptr);s.initialized.store(true,std::memory_order_release);
}
dc_xr::InputSink native_mouse_sink(){
 auto& s=state();const std::lock_guard<std::mutex> lock(s.activation);
 if(!s.initialized.load(std::memory_order_acquire))return {};
 if(!s.enabled.load(std::memory_order_acquire)){
  s.consumer.reset(s.generation.load(std::memory_order_acquire));
  s.enabled.store(true,std::memory_order_release);
 }
 return {&s,[](void* user,const dc_xr::Target& target){
   auto& value=*static_cast<NativeState*>(user);
   if(value.enabled.load(std::memory_order_acquire))value.consumer.submit(target);
  },[](void* user,std::uint64_t epoch){static_cast<NativeState*>(user)->consumer.reset(epoch);}};
}
void disable_native_mouse(){
 auto& s=state();const std::lock_guard<std::mutex> lock(s.activation);
 s.enabled.store(false,std::memory_order_release);s.consumer.reset(0);
}
bool acknowledge_native_mouse_limits(int yaw_limit,int pitch_limit){
 if(!matches_title())return false;
 const auto* ram=GetMemPtr(0x0c000000u,RamBytes);
 if(!read_source(ram,RamBytes,settings.content.gameId,yaw_limit,pitch_limit).code_verified)return false;
 auto& s=state();s.limits.store((std::uint64_t(yaw_limit)<<32)|unsigned(pitch_limit),std::memory_order_release);
 s.consumer.reset(s.generation.load(std::memory_order_acquire));
 return true;
}
dc_menu::MenuFacts native_menu_snapshot(){auto& s=state();const std::lock_guard<std::mutex> lock(s.menu_mutex);return s.menu_snapshot;}
bool poll_native_mouse(std::uint8_t& buttons,int& x,int& y,int& wheel){
 auto& s=state();
 if(!s.enabled.load(std::memory_order_acquire))return false;
 if(!matches_title()){
  s.consumer.reset(s.generation.load(std::memory_order_acquire));return false;
 }
 const auto generation=s.generation.load(std::memory_order_acquire);
 const auto caps=s.limits.load(std::memory_order_acquire);
 const auto* ram=GetMemPtr(0x0c000000u,RamBytes);
 const auto facts=read_source(ram,RamBytes,settings.content.gameId,int(caps>>32),int(caps&0xffffffffu));
 Feedback feedback;feedback.code_verified=facts.code_verified;feedback.mouse_device=true;
 feedback.generation=generation;feedback.poll_sequence=++s.sequence;
 feedback.native_seconds=sh4_sched_now64()/double(SH4_MAIN_CLOCK);
 feedback.yaw=facts.yaw;feedback.pitch=facts.pitch;feedback.gain=source_hint(facts.context);
 const auto menu=read_menu_source(ram,RamBytes,settings.content.gameId);
 feedback.menu_navigation_verified=menu.code_verified&&menu.mouse_present;
 feedback.main_menu_verified=menu.main_menu_verified;feedback.menu_selected=menu.selection;feedback.menu_count=menu.count;feedback.menu_native_wheel=menu.wheel;
 auto typed_menu=dc_menu::read_source(ram,RamBytes,settings.content.gameId);
 {const std::lock_guard<std::mutex> lock(s.menu_mutex);
  // Page and row are acknowledged transaction state, not a new epoch. A code
  // failure or family/layout change invalidates all captured host actions.
  if(typed_menu.kind!=s.menu_snapshot.kind||typed_menu.verified!=s.menu_snapshot.verified||typed_menu.source_context!=s.menu_snapshot.source_context)++s.menu_revision;
  typed_menu.context_revision=s.menu_revision;s.menu_snapshot=typed_menu;
 }
 feedback.menu=typed_menu;
 feedback.yaw_min=-double(facts.yaw_limit);feedback.yaw_max=facts.yaw_limit;
 feedback.pitch_min=-double(facts.pitch_limit);feedback.pitch_max=facts.pitch_limit;
 auto packet=s.consumer.poll(host_seconds(),feedback);
 if(generation!=s.generation.load(std::memory_order_acquire)||!s.enabled.load(std::memory_order_acquire))packet={};
 log_menu(packet,feedback,menu);
 if(typed_menu.kind==dc_menu::MenuKind::ModeSelect){
  static unsigned reports=0;
  static int last_status=-1,last_row=-1,last_column=-1;
  const int status=int(packet.menu_action_status);
  if(reports<120&&(status!=last_status||typed_menu.row!=last_row||typed_menu.column!=last_column)){
   ++reports;last_status=status;last_row=typed_menu.row;last_column=typed_menu.column;
   NOTICE_LOG(INPUT,"SS_MODE_NATIVE native_s=%.3f aim=%d row=%d col=%d yaw=%.1f pitch=%.1f status=%d dx=%d dy=%d buttons=%u",
       feedback.native_seconds,int(typed_menu.aim_verified),typed_menu.row,typed_menu.column,
       typed_menu.native_yaw,typed_menu.native_pitch,status,packet.dx,packet.dy,unsigned(packet.pressed));
  }
 }
#if defined(_WIN32) && !defined(LIBRETRO) && !defined(USE_OPENXR)
 dc_menu_pointer_probe::observe_native_packet(packet,feedback,menu);
#endif
 buttons=std::uint8_t((buttons|OwnedButtons)&~packet.pressed);
 x=packet.dx;y=packet.dy;wheel=packet.wheel;
 return true;
}
}}
