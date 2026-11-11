// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Windows,!LIBRETRO,!USE_OPENXR sole synthetic producer. Actual production
// bridge/sink/consumer/native feedback own packets. No direct Maple override.
#include "dc_mouse_native.h"
#include "dc_mouse_menu_source.h"
#include "dc_menu_typed_probe.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <tuple>
namespace silent_scope { namespace dc_menu_pointer_probe {
inline bool enabled(){const auto* p=std::getenv("SILENT_SCOPE_DC_MENU_POINTER_PROBE");return p&&std::string_view(p)=="1";}
class Policy {
 int requested_item_{3};
 bool started_{},cancelled_{},requested_{},completed_{};unsigned menu_samples_{};double first_{},last_{},trigger_at_{};
public:
 Policy()=default;
 explicit Policy(int item):requested_item_(item>=0&&item<4?item:3){}
 dc_xr::Target target(double native,double host,const dc_mouse::MenuSourceFacts& facts){
  dc_xr::Target out;out.host_seconds=host;out.hand=1;out.native_ui=true;out.point_valid=true;out.native_point={0,0,-1};
  if(!std::isfinite(native)||!std::isfinite(host)||native<0||host<0)return out;
  if(!started_){first_=last_=native;started_=true;}
  if(native<last_)cancelled_=true;
  last_=native;const double elapsed=native-first_;
  if(requested_&&!facts.main_menu_verified)completed_=true;
  if(cancelled_||completed_||elapsed>=40)return out;
  out.focused=out.tracked=true;out.ui_main_menu=facts.main_menu_verified;
  if(!facts.main_menu_verified){
   const bool preparing=facts.code_verified&&facts.table_pointer==0x8c0e1a48u&&facts.count==4;
   // Diagnostic only: continue until late warnings clear, varying native-frame
   // phase rather than repeatedly sampling one30Hz guest phase at integer seconds.
   out.ui_select=!preparing&&elapsed<38&&std::fmod(elapsed,1.017)<.2;
   out.start=!preparing&&elapsed>=20&&elapsed<20.2;
  }else{
   ++menu_samples_;
   if(elapsed>=23){out.ui_pointer_item=requested_item_;
    if(!requested_&&menu_samples_>1&&facts.selection==0&&facts.wheel==0){requested_=true;trigger_at_=native;}
    out.ui_select=requested_&&native-trigger_at_<.12;
   }
  }
  return out;
 }
 bool requested()const{return requested_;}
};
struct Runtime {bool initialized{};Policy policy;unsigned records{},packet_records{};int selected{-999};bool was_main{},was_trigger{},was_requested{};
 int packet_status{-1},packet_pressed{-1},packet_state{-1},packet_code{-1},packet_count{-1};std::uint64_t packet_generation{};};
inline Runtime& runtime(){static Runtime value;return value;}
inline void initialize_menu_pointer_probe(std::string_view title){
 if(!enabled()||title!="T9507N")return;
 auto& r=runtime();r=Runtime{};
 if(const auto* text=std::getenv("SILENT_SCOPE_DC_MENU_POINTER_ITEM")){
  char* end=nullptr;const auto item=std::strtol(text,&end,10);
  if(end&&*end=='\0'&&item>=0&&item<4)r.policy=Policy(int(item));
 }
 for(const char* name:{"SILENT_SCOPE_DC_MENU_NAV_PROBE","SILENT_SCOPE_DC_MOUSE_CONSUMER_PROBE","SILENT_SCOPE_DC_VIEW_RUNTIME_PROBE"}){
  const auto* p=std::getenv(name);if(p&&std::string_view(p)=="1"){
   std::fprintf(stderr,"SS_MENU_PROBE blocked_conflict=%s\n",name);return;
  }
 }
 dc_mouse::initialize_native_mouse();dc_xr::set_input_sink(dc_mouse::native_mouse_sink());r.initialized=true;
 std::fprintf(stderr,"SS_MENU_PROBE initialized synthetic_native=true xr=false actual_native_sink=true\n");
}
// Emulation-thread sole producer in the explicit native experiment only.
// Caller continues to the existing poll_native_mouse; this never touches output.
inline void update_menu_pointer_probe(std::string_view title,double native,const std::uint8_t* ram,std::size_t bytes){
 auto& r=runtime();if(!enabled()||!r.initialized||title!="T9507N")return;
 const auto facts=dc_mouse::read_menu_source(ram,bytes,title);
 // Read-only controller snapshots continue after input becomes neutral. Raw
 // address names are diagnostic evidence, never an input/layout capability.
 static unsigned screen_records{};static double screen_at{-1};
 if(ram&&bytes==16u*1024*1024&&screen_records<80&&native-screen_at>=1){
  const auto word=[&](unsigned address){const auto a=address&0xffffffu;
   return unsigned(ram[a])|(unsigned(ram[a+1])<<8)|(unsigned(ram[a+2])<<16)|(unsigned(ram[a+3])<<24);};
  std::fprintf(stderr,"SS_MENU_SCREEN native_s=%.9f code=%u state=%u table=%08x selected=%d/%d scene=%08x",
   native,unsigned(facts.code_verified),facts.main_menu_state,unsigned(facts.table_pointer),facts.selection,facts.count,word(0x8c1b014c));
  for(unsigned a=0x8c272004;a<=0x8c272030;a+=4)std::fprintf(stderr," raw%06x=%08x",a&0xffffffu,word(a));
  for(unsigned a=0x8c272440;a<=0x8c272460;a+=4)std::fprintf(stderr," raw%06x=%08x",a&0xffffffu,word(a));
  std::fputc('\n',stderr);screen_at=native;++screen_records;
 }
 const double host=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
 auto target=r.policy.target(native,host,facts);dc_xr::Target emitted;
 const auto menu=dc_mouse::native_menu_snapshot();
 // Boot dialogs now have explicit typed authority too. This is the original
 // synthetic boot gesture, not a replacement of any native feedback.
 if(menu.verified&&menu.kind==dc_menu::MenuKind::Dialog){
  target.ui_action.kind=menu.kind;target.ui_action.context_revision=menu.context_revision;
  if(target.ui_select){target.ui_action.verb=dc_menu::MenuVerb::Confirm;target.ui_action.row=menu.source_context==1?0:-1;}
 }
 static dc_menu_typed_probe::Policy typed{dc_menu_typed_probe::mode_from_environment()};
 if(r.policy.requested())if(auto next=typed.target(native,host,menu))target=*next;
 dc_xr::input_bridge().submit(target,&emitted);
 static unsigned typed_records{};static std::tuple<int,int,int,int,int,int,int> previous_typed{};
 const auto typed_key=std::make_tuple(int(menu.kind),menu.row,menu.column,menu.page,menu.value,typed.stage(),int(emitted.ui_select));
 if(typed.owns()&&typed_records<128&&typed_key!=previous_typed){
  std::fprintf(stderr,"SS_TYPED_MENU native_s=%.9f kind=%d verified=%u active=%u interactive=%u revision=%llu row=%d column=%d page=%d value=%d stage=%d trigger=%u verb=%d requested_row=%d requested_column=%d requested_page=%d done=%u cancelled=%u\n",
   native,int(menu.kind),unsigned(menu.verified),unsigned(menu.context_active),unsigned(menu.interactive),static_cast<unsigned long long>(menu.context_revision),menu.row,menu.column,menu.page,menu.value,
   typed.stage(),unsigned(emitted.ui_select),int(emitted.ui_action.verb),emitted.ui_action.row,emitted.ui_action.column,emitted.ui_action.page,unsigned(typed.completed()),unsigned(typed.cancelled()));++typed_records;
 }
 previous_typed=typed_key;
 if(r.records<48&&(facts.main_menu_verified!=r.was_main||facts.selection!=r.selected||emitted.ui_select!=r.was_trigger||r.policy.requested()!=r.was_requested)){
  std::fprintf(stderr,"SS_MENU_PROBE synthetic_native=true xr=false native_s=%.9f source=%u state=%u table=%08x selected=%d/%d native_wheel=%d item=%d trigger=%u requested=%u\n",
   native,unsigned(facts.main_menu_verified),facts.main_menu_state,unsigned(facts.table_pointer),facts.selection,facts.count,facts.wheel,
   emitted.ui_pointer_item,unsigned(emitted.ui_select),unsigned(r.policy.requested()));++r.records;
 }
 r.selected=facts.selection;r.was_main=facts.main_menu_verified;r.was_trigger=emitted.ui_select;r.was_requested=r.policy.requested();
}
inline void observe_native_packet(const dc_mouse::Packet& packet,const dc_mouse::Feedback& feedback,const dc_mouse::MenuSourceFacts& menu){
 auto& r=runtime();if(!enabled()||!r.initialized)return;
 static unsigned typed_records{};static std::tuple<int,int,int,int,int,int,int,int,int,int> previous_typed{};
 const auto& f=feedback.menu;
 const auto typed_key=std::make_tuple(int(f.kind),f.row,f.column,f.page,f.value,int(f.interactive),packet.dx,packet.wheel,int(packet.pressed),int(packet.menu_action_status));
 if(f.kind!=dc_menu::MenuKind::None&&typed_records<160&&typed_key!=previous_typed){
  std::fprintf(stderr,"SS_TYPED_PACKET native_s=%.9f kind=%d verified=%u active=%u interactive=%u row=%d column=%d page=%d value=%d x=%d wheel=%d buttons=%u policy=%d\n",
   feedback.native_seconds,int(f.kind),unsigned(f.verified),unsigned(f.context_active),unsigned(f.interactive),f.row,f.column,f.page,f.value,packet.dx,packet.wheel,unsigned(packet.pressed),int(packet.menu_action_status));++typed_records;
 }
 previous_typed=typed_key;
 const int status=int(packet.status),pressed=packet.pressed,code=feedback.code_verified;
 if(r.packet_records<128&&(status!=r.packet_status||pressed!=r.packet_pressed||int(menu.main_menu_state)!=r.packet_state
      ||code!=r.packet_code||menu.count!=r.packet_count||feedback.generation!=r.packet_generation)){
  std::fprintf(stderr,"SS_MENU_PACKET synthetic_native=true xr=false native_s=%.9f generation=%llu status=%d pressed=%u code=%u mouse=%u state=%u selected=%d/%d menu_source=%u wheel=%d\n",
   feedback.native_seconds,static_cast<unsigned long long>(feedback.generation),status,unsigned(packet.pressed),unsigned(feedback.code_verified),unsigned(feedback.mouse_device),
   menu.main_menu_state,menu.selection,menu.count,unsigned(menu.main_menu_verified),packet.wheel);++r.packet_records;
 }
 r.packet_status=status;r.packet_pressed=pressed;r.packet_state=int(menu.main_menu_state);r.packet_code=code;r.packet_count=menu.count;r.packet_generation=feedback.generation;
}
}}
