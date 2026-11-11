// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_menu_actions.h"
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <string_view>
namespace silent_scope { namespace dc_menu {
// US T9507N owned boot SHA2563cf1879f...; source code and state READS only.
class Reader {
 const std::uint8_t* ram_;std::size_t bytes_;
public:
 Reader(const std::uint8_t* ram,std::size_t bytes):ram_(ram),bytes_(bytes){}
 bool valid()const{return ram_&&bytes_==16u*1024*1024;}
 bool address(std::uint32_t a,std::size_t n)const{return valid()&&(a&0xff000000u)==0x8c000000u&&n<=bytes_&&(a&0xffffffu)<=bytes_-n;}
 std::uint32_t u32(std::uint32_t a)const{std::uint32_t n{};if(address(a,4))std::memcpy(&n,ram_+(a&0xffffffu),4);return n;}
 std::uint16_t u16(std::uint32_t a)const{std::uint16_t n{};if(address(a,2))std::memcpy(&n,ram_+(a&0xffffffu),2);return n;}
 std::uint8_t u8(std::uint32_t a)const{return address(a,1)?ram_[a&0xffffffu]:0;}
 std::int32_t s32(std::uint32_t a)const{return static_cast<std::int32_t>(u32(a));}
 std::int16_t s16(std::uint32_t a)const{return static_cast<std::int16_t>(u16(a));}
 bool neutral_float(std::uint32_t a)const{const auto n=u32(a);return (n&0x7fffffffu)==0;}
};
struct ControlGuard {std::uint32_t address,bytes;std::uint64_t hash;};
inline constexpr ControlGuard CommonDirections{0x8c08cb20,0x404,0x61f57ec33b61c4dbull};
inline constexpr ControlGuard MouseAxes{0x8c08c54c,0xec,0xe80a522abf73514cull};
inline constexpr ControlGuard AxisRepeat{0x8c08cf88,0x518,0x18a7829bcf96233full};
inline constexpr ControlGuard MouseFrame{0x8c01d5b8,0x7c,0x2a1686b5514298bdull};
inline constexpr ControlGuard TrainingControl{0x8c092b46,0x188,0x493894cec9b51bebull};
inline constexpr ControlGuard OptionsControl{0x8c08992c,0x1864,0x11d7ad3ae6a2b93cull};
inline constexpr ControlGuard OptionsRecords{0x8c08b3c0,0x920,0x15650a80d8738dacull};
inline constexpr ControlGuard RankingControl{0x8c0904aa,0x304,0xc242deabfc47b3beull};
inline constexpr ControlGuard RankingMasks{0x8c0a4a90,0x24,0x72279c8e86f85da1ull};
inline constexpr ControlGuard PauseControl{0x8c0805ee,0x2aa,0x94e458ed8e9e9c16ull};
inline constexpr ControlGuard PauseRows{0x8c091110,0xfc,0x04290d286b21ad95ull};
inline constexpr ControlGuard GenericSelector{0x8c086a20,0x360,0xf53a8354edc96614ull};
inline constexpr ControlGuard NoVmuDialog{0x8c08e4e0,0x576,0xe680530c86b9ad85ull};
inline constexpr ControlGuard VmuStatus{0x8c01d800,0x4c,0xf5487295dc71f7fdull};
inline constexpr ControlGuard VmuPointer{0x8c01d8c0,4,0xae268326c1e2c0ddull};
inline constexpr ControlGuard SaveDialog{0x8c08d4fe,0x9e6,0xeb0c363b1dca33b4ull};
inline constexpr ControlGuard LoadDialog{0x8c08dee4,0x5fc,0x692e4d5b7aecf978ull};
inline constexpr ControlGuard AutoSaveDialog{0x8c08ea76,0x576,0x3c2a714560b652e6ull};
inline constexpr ControlGuard ModePicker{0x8c07045e,0x6ba,0xaa33e2390e71d43bull};
inline constexpr ControlGuard ModeUpdater{0x8c071f8a,0xde,0x67e6924dd13e8682ull};
inline constexpr ControlGuard ModeOwner{0x8c076100,0x12c,0xf56552becca1f1dcull};
inline constexpr ControlGuard ModeMouseAim{0x8c073b80,0x2fe,0xb720ec9b33465133ull};
inline constexpr ControlGuard ModeMouseTables{0x8c0c7a64,0x3c,0x2a32ee5499087bdcull};
inline constexpr ControlGuard StoryRoutePicker{0x8c070eda,0x31a,0x19c5af459fd1062cull};
inline constexpr ControlGuard StoryRouteOwner{0x8c075ef6,0x5a,0x42cd95c65b846e47ull};
inline constexpr ControlGuard InvasionRouteOwner{0x8c07468c,0x1b0,0xec8e9a8b67165981ull};
inline constexpr ControlGuard InvasionRoutePicker{0x8c0711f8,0x3ac,0x73ef36813cf919edull};
inline constexpr ControlGuard OutdoorStagePicker{0x8c072098,0x7b8,0x6f5d1815fe3516caull};
inline constexpr ControlGuard OutdoorStageOwner{0x8c080480,0xb4,0x4a9365e07518e72cull};
inline bool arcade_mode_owner(const Reader& r){
 return r.u8(0x8c1b014c)==6&&r.u8(0x8c1b014d)==1&&r.u8(0x8c1b014e)==1&&r.u16(0x8c2214c8)==0;
}
inline bool story_route_owner(const Reader& r){
 return r.u8(0x8c1b014c)==6&&r.u8(0x8c1b014d)==5&&r.u8(0x8c1b015e)==0
     &&r.u16(0x8c2214c8)==1&&(r.u32(0x8c1b088c)&0x00404000u)==0x00404000u;
}
inline bool outdoor_stage_owner(const Reader& r){
 return r.u32(0x8c1b014c)==0x01070208u&&r.u16(0x8c2214c8)==6
     &&(r.u32(0x8c1b088c)&0x4000u)!=0;
}
inline bool invasion_route_owner(const Reader& r){
 return r.u32(0x8c1b014c)==0x01070406u&&r.u8(0x8c1b015e)==0&&r.u16(0x8c1b0884)==4
     &&r.u16(0x8c2214c8)==4&&(r.u32(0x8c1b088c)&0x4000u)!=0;
}
// The game draws these menus with its main lens, including entry animations.
inline bool stock_picker_projection(const Reader& r){
 return arcade_mode_owner(r)||story_route_owner(r)||invasion_route_owner(r)||outdoor_stage_owner(r);
}
inline bool code_matches(const Reader& r,ControlGuard g){
 if(!r.address(g.address,g.bytes))return false;
 std::uint64_t h=1469598103934665603ull;
 for(std::uint32_t n=0;n<g.bytes;++n){h^=r.u8(g.address+n);h*=1099511628211ull;}
 return h==g.hash;
}
// Pure field decoding is deliberately NOT an authorization: verified stays
// false here. read_source checks every required live code/literal guard first.
inline MenuFacts decode_state(const Reader& r,MenuKind kind){
 MenuFacts f;if(!r.valid())return f;f.kind=kind;
 f.native_wheel=r.s32(0x8c1e1d80);
 f.navigation_neutral=f.native_wheel==0&&r.neutral_float(0x8c1e1d78);
 if(kind==MenuKind::Training){
  f.row=r.s32(0x8c0f31d4);f.column=r.s32(0x8c0f31d8);f.row_count=4;f.column_count=3;
  const auto phase=r.u32(0x8c272568);
  if(f.row<0||f.row>=4||f.column<0||f.column>=3||phase>3)return {};
  f.context_active=f.interactive=phase==1;f.confirm_button=NativeLeft;f.back_button=NativeRight;
 }else if(kind==MenuKind::Options){
  f.page=r.s32(0x8c272004);f.row=r.s32(0x8c272018);f.page_count=6;
  if(f.page<0||f.page>=6)return {};
  f.row_count=r.s32(0x8c2722f0+unsigned(f.page)*4);
  const auto phase=r.u32(0x8c272010);
  if(f.row_count<0||f.row_count>6||f.row<0||f.row>f.row_count||phase>3)return {};
  f.context_active=phase==1;f.interactive=f.context_active&&r.u32(0x8c272008)!=0&&r.u32(0x8c272028)==0&&r.u32(0x8c272030)==0;
  f.back_button=NativeRight;f.exit_button=NativeMiddle;
  if(f.row>0){
   const auto record=0x8c272058u+unsigned(f.page)*72+unsigned(f.row-1)*12;
   const auto pointer=r.u32(record);
   // Source-authored records target native configuration or private option
   // working values; arbitrary RAM addresses cannot become an adjust ACK.
   const bool allowed=(pointer>=0x8c26fdc8u&&pointer<=0x8c27196cu)
       ||(pointer>=0x8c2722a8u&&pointer<=0x8c2722ecu);
   if(allowed&&(pointer&3u)==0){
    f.value=r.s32(pointer);f.value_min=r.s16(record+4);f.value_max=r.s16(record+6);
    f.value_verified=f.value_min<=f.value_max&&f.value>=f.value_min&&f.value<=f.value_max;
    const auto device=r.u16(0x8c219ea0);
    if(f.value_verified&&device<=2){
     f.value_neighbors_verified=true;f.previous_value=f.value>f.value_min?f.value-1:f.value;
     f.next_value=f.value<f.value_max?f.value+1:f.value;
     // Original Continue option skips device-specific unavailable values.
     if(f.page==0&&pointer==0x8c26fdd0u){
      if(device==1||device==2){if(f.value==2)f.next_value=5;if(f.value==5)f.previous_value=2;}
      else{if(f.value==3&&r.u32(0x8c2722ac)!=1)f.next_value=5;
       if(f.value==5)f.previous_value=r.u32(0x8c2722ac)!=0?4:3;}
     }
    }
   }
  }
 }else if(kind==MenuKind::Ranking){
  f.page=r.s32(0x8c272450);f.page_count=14;const auto phase=r.u32(0x8c272460);
  if(f.page<0||f.page>=14||phase>3)return {};
  f.context_active=phase==1;f.interactive=f.context_active&&r.u32(0x8c27244c)==1;f.back_button=NativeRight;
  const auto available=[&](int page){if(page<2||page>=11)return true;
   const auto mask=r.u32(0x8c0a4a90+unsigned(page-2)*4);return (r.u32(0x8c271970)&mask)==mask;};
  for(int step=1;step<14;++step){int p=(f.page+step)%14;if(available(p)){f.next_page=p;break;}}
  for(int step=1;step<14;++step){int p=(f.page-step+14)%14;if(available(p)){f.previous_page=p;break;}}
 }else if(kind==MenuKind::Pause){
  // Native authored labels0=heading,1..3 selectable; expose zero-based actions.
  f.row=r.s32(0x8c272548)-1;f.row_count=3;
  if(f.row<0||f.row>=3)return {};
  f.context_active=f.interactive=r.u32(0x8c272544)==1&&r.u32(0x8c26fda8)==1&&r.u32(0x8c0ee860)==0;
  f.confirm_button=NativeMiddle; // 080720 calls08c454; Left is not consumed.
 }else if(kind==MenuKind::ModeSelect){
  const bool route=story_route_owner(r);
  const bool invasion=invasion_route_owner(r);
  const bool outdoor=outdoor_stage_owner(r);
  const bool arcade=arcade_mode_owner(r);
  if(!route&&!invasion&&!outdoor&&!arcade)return {};
  const auto phase=r.u8(0x8c2214c0),inner=r.u8(0x8c2214c1);
  f.mode_type=outdoor?6:invasion?4:route?1:0;f.variant=r.s16(0x8c2214cc);f.row=r.s16(0x8c2214de);
  f.row_count=outdoor?9:invasion?2:route?3:5;f.column_count=arcade?2:0;
  if(phase>2||inner>(arcade?5:6)||f.row>=f.row_count||
     (f.row<0&&(!outdoor||f.row!=-1||inner>=4))||(outdoor&&f.variant!=10))return {};
  f.source_context=outdoor?OutdoorStageContext:invasion?InvasionRouteContext:route?StoryRouteContext:ArcadeModeContext;
  f.context_active=f.interactive=phase==1&&inner==(arcade?3:4);
  f.native_pitch=r.s16(0x8c1b0860);f.native_yaw=r.s16(0x8c1b0862);
  f.column=arcade&&f.row==1?(f.native_yaw<0?1:0):-1;
  if(outdoor){
   const auto progress=r.u32(0x8c271970);
   for(int stage=0;stage<f.row_count;++stage)if(progress>>stage)f.available_regions|=1u<<stage;
  }else f.available_regions=(1u<<f.row_count)-1;
  f.confirm_button=NativeLeft;f.back_button=route||invasion?0:NativeRight;
  const auto peripheral=r.u32(0x8c1d54ac);const auto speed=r.u32(0x8c26fe1c);
  f.aim_profile=1469598103934665603ull;
  for(auto value:{r.u32(0x8c271ffc),speed,r.u32(0x8c26fe18),std::uint32_t(r.u8(0x8c26fdc7)&0x10),
      std::uint32_t(r.u8(0x8c1b015c)),r.u32(0x8c1b013c),r.u32(0x8c1b088c)&0x4000u}){
   f.aim_profile^=value;f.aim_profile*=1099511628211ull;
  }
  if(speed<=2){constexpr double speeds[]{.5,1,2};const double gain=200.*speeds[speed]/16.;
   f.aim_gain_yaw=(r.u8(0x8c26fdc7)&0x10)?gain:-gain;f.aim_gain_pitch=r.u32(0x8c26fe18)?gain:-gain;}
  // Force the real mouse/normal, unscoped branch. D-pad and alternate absolute
  // paths cannot acquire this200-based gain certificate.
  if(r.address(peripheral,0x34)&&r.s32(0x8c1e1d68)>=0&&speed<=2&&r.u32(0x8c271ffc)==0
      &&r.u8(0x8c1b015c)>=128&&r.u32(0x8c1b013c)==0&&(r.u32(0x8c1b088c)&0x4000)
      &&!(r.u32(peripheral+8)&0xf0)&&!(r.u32(peripheral+8)&r.u32(0x8c272328))){
   f.aim_verified=true;
  }
 }else if(kind==MenuKind::Dialog){
  // Actual023: source-owned informational errors only. Shared phase alone
  // cannot authorize overwrite/format or any VMU-present operation.
  if(r.u8(0x8c1b014d)==5&&r.u32(0x8c272030)==1){
   if(r.u32(0x8c272004)!=4||r.u32(0x8c272298)!=2||r.u32(0x8c1e3c10)!=0||r.u32(0x8c26fe14)!=1)return {};
   const auto row=r.u32(0x8c272018),phase=r.u32(0x8c272430);
   const auto operation=row==1?0x8c27243cu:row==2?0x8c272440u:0x8c272448u;
   if(row<1||row>3||phase!=(row==3?2u:3u)||r.u32(operation)!=7)return {};
   f.source_context=row+2;f.context_active=f.interactive=true;f.confirm_button=NativeLeft;return f;
  }
  // Photosensitivity is a plain acknowledgement, not the lingering YES/NO
  // table from the preceding question. Original08e8f8 accepts Left/Right/Start.
  if(r.u32(0x8c272434)==5&&r.u32(0x8c26fe14)==1){
   f.source_context=2;f.context_active=f.interactive=true;f.confirm_button=NativeLeft;return f;
  }
  // Only the source-proven English start-without-VMU question. Two generic
  // rows alone do not authorize a different warning, overwrite or format.
  if(r.u32(0x8c272434)!=2||r.u32(0x8c272444)!=7||r.u32(0x8c1e3c10)!=0
      ||r.u32(0x8c26fe14)!=1||r.u32(0x8c27203c)!=0x8c0f2dc8u||r.u32(0x8c272038)!=2
      ||r.u32(0x8c272040)!=6||r.u32(0x8c272044)!=45||r.u32(0x8c272048)!=32)return {};
  f.row=r.s32(0x8c272034);if(f.row<0||f.row>=2)return {};
  f.row_count=2;f.source_context=1;f.context_active=f.interactive=true;f.confirm_button=NativeLeft;
 }else return {};
 return f;
}
inline MenuFacts read_source(const std::uint8_t* ram,std::size_t bytes,std::string_view title){
 Reader r(ram,bytes);if(title!="T9507N"||!r.valid())return {};
 if(!code_matches(r,CommonDirections)||!code_matches(r,MouseAxes)||!code_matches(r,AxisRepeat)||!code_matches(r,MouseFrame))return {};
 if(r.s32(0x8c1e1d68)<0)return {};
 if(outdoor_stage_owner(r)){
  if(!code_matches(r,OutdoorStageOwner)||!code_matches(r,OutdoorStagePicker)
      ||!code_matches(r,ModeUpdater)||!code_matches(r,ModeMouseAim)||!code_matches(r,ModeMouseTables))return {};
  auto outdoor=decode_state(r,MenuKind::ModeSelect);
  outdoor.verified=outdoor.kind==MenuKind::ModeSelect;
  return outdoor;
 }
 if(story_route_owner(r)){
  if(!code_matches(r,StoryRouteOwner)||!code_matches(r,StoryRoutePicker)
      ||!code_matches(r,ModeUpdater)||!code_matches(r,ModeMouseAim)||!code_matches(r,ModeMouseTables))return {};
  auto route=decode_state(r,MenuKind::ModeSelect);
  route.verified=route.kind==MenuKind::ModeSelect;
  return route;
 }
 if(invasion_route_owner(r)){
  if(!code_matches(r,InvasionRouteOwner)||!code_matches(r,InvasionRoutePicker)
      ||!code_matches(r,ModeUpdater)||!code_matches(r,ModeMouseAim)||!code_matches(r,ModeMouseTables))return {};
  auto route=decode_state(r,MenuKind::ModeSelect);
  route.verified=route.kind==MenuKind::ModeSelect;
  return route;
 }
 MenuKind kind=MenuKind::None;
 if(r.u8(0x8c1b014d)==5&&r.u32(0x8c272030)==1){
  if(!code_matches(r,OptionsControl)||!code_matches(r,OptionsRecords)||!code_matches(r,VmuStatus)||!code_matches(r,VmuPointer))return {};
  const auto row=r.u32(0x8c272018);
  const auto controller=row==1?SaveDialog:row==2?LoadDialog:AutoSaveDialog;
  if(!code_matches(r,controller))return {};
  auto modal=decode_state(r,MenuKind::Dialog);
  if(modal.kind==MenuKind::Dialog){modal.verified=true;return modal;}
  // A modal Options owner cannot fall through to stale boot warning fields.
  auto options=decode_state(r,MenuKind::Options);options.verified=options.kind==MenuKind::Options;return options;
 }
 if(r.u32(0x8c272434)==2||r.u32(0x8c272434)==5){
  if(!code_matches(r,GenericSelector)||!code_matches(r,NoVmuDialog)||!code_matches(r,VmuStatus))return {};
  const auto dialog=decode_state(r,MenuKind::Dialog);
  if(dialog.kind!=MenuKind::None){auto f=dialog;f.verified=true;return f;}
 }
 if(r.u8(0x8c1b014c)==6&&r.u8(0x8c1b014d)==1){
  if(!code_matches(r,ModePicker)||!code_matches(r,ModeUpdater)||!code_matches(r,ModeOwner)
      ||!code_matches(r,ModeMouseAim)||!code_matches(r,ModeMouseTables))return {};
  kind=MenuKind::ModeSelect;
 }else if(r.u32(0x8c26fda8)==1&&r.u32(0x8c0ee860)==0){
  if(!code_matches(r,PauseControl)||!code_matches(r,PauseRows))return {};
  kind=MenuKind::Pause;
 }else if(r.u8(0x8c1b014d)==4&&r.u32(0x8c0d19a8)==3){
  if(!code_matches(r,TrainingControl))return {};
  kind=MenuKind::Training;
 }else if(r.u8(0x8c1b014d)==5){
  if(!code_matches(r,OptionsControl)||!code_matches(r,OptionsRecords))return {};
  kind=MenuKind::Options;
 }else if(r.u8(0x8c1b014d)==6){
  if(!code_matches(r,RankingControl)||!code_matches(r,RankingMasks))return {};
  kind=MenuKind::Ranking;
 }
 auto f=decode_state(r,kind);f.verified=f.kind!=MenuKind::None;return f;
}
}}
