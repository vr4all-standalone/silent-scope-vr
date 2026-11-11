// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_menu_source.h"
#include <cassert>
#include <cstring>
#include <vector>
using namespace silent_scope::dc_menu;
int main(){
 std::vector<std::uint8_t> ram(16u*1024*1024);
 const auto put=[&](std::uint32_t a,std::uint32_t n){std::memcpy(ram.data()+(a&0xffffff),&n,4);};
 Reader r(ram.data(),ram.size());
 assert(!read_source(ram.data(),ram.size(),"T9507N").verified); // zeroed code never authorizes
 put(0x8c0f31d4,3);put(0x8c0f31d8,2);put(0x8c272568,1);
 auto f=decode_state(r,MenuKind::Training);assert(f.kind==MenuKind::Training&&f.row==3&&f.column==2&&f.interactive&&!f.verified);
 put(0x8c0f31d8,3);assert(decode_state(r,MenuKind::Training).kind==MenuKind::None);
 put(0x8c272010,1);put(0x8c272008,1);put(0x8c272004,0);put(0x8c272018,1);put(0x8c2722f0,4);
 put(0x8c272058,0x8c26fdf0);put(0x8c27205c,7u<<16);put(0x8c26fdf0,3);
 f=decode_state(r,MenuKind::Options);assert(f.row==1&&f.page==0&&f.row_count==4&&f.value_verified&&f.value==3&&f.value_min==0&&f.value_max==7);
 put(0x8c272028,1);assert(!decode_state(r,MenuKind::Options).interactive);put(0x8c272028,0);
 put(0x8c219ea0,0x07d00001);put(0x8c272018,4);put(0x8c27207c,0x8c26fdd0);put(0x8c272080,5u<<16);put(0x8c26fdd0,2);
 f=decode_state(r,MenuKind::Options);assert(f.value_neighbors_verified&&f.next_value==5);
 put(0x8c26fdd0,5);assert(decode_state(r,MenuKind::Options).previous_value==2);put(0x8c272018,1);
 put(0x8c272058,0x8c123456);assert(!decode_state(r,MenuKind::Options).value_verified);
 put(0x8c272004,6);assert(decode_state(r,MenuKind::Options).kind==MenuKind::None);
 put(0x8c272450,0);put(0x8c272460,1);put(0x8c27244c,1);put(0x8c271970,0);
 for(unsigned n=0;n<9;++n)put(0x8c0a4a90+n*4,1u<<n);
 f=decode_state(r,MenuKind::Ranking);assert(f.page==0&&f.next_page==1&&f.previous_page==13);
 put(0x8c272450,1);f=decode_state(r,MenuKind::Ranking);assert(f.next_page==11);
 put(0x8c271970,1);f=decode_state(r,MenuKind::Ranking);assert(f.next_page==2);
 put(0x8c27244c,0);assert(!decode_state(r,MenuKind::Ranking).interactive);
 put(0x8c272548,2);put(0x8c272544,1);put(0x8c26fda8,1);put(0x8c0ee860,0);
 f=decode_state(r,MenuKind::Pause);assert(f.row==1&&f.row_count==3&&f.confirm_button==NativeMiddle&&f.back_button==0);
 put(0x8c272548,0);assert(decode_state(r,MenuKind::Pause).kind==MenuKind::None);
 put(0x8c272434,2);put(0x8c272444,7);put(0x8c1e3c10,0);put(0x8c26fe14,1);
 put(0x8c27203c,0x8c0f2dc8);put(0x8c272038,2);put(0x8c272040,6);put(0x8c272044,45);put(0x8c272048,32);put(0x8c272034,1);
 f=decode_state(r,MenuKind::Dialog);assert(f.kind==MenuKind::Dialog&&f.source_context!=0&&f.row==1&&f.row_count==2&&f.confirm_button==NativeLeft&&f.back_button==0);
 put(0x8c272444,1);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);put(0x8c272444,7);
 put(0x8c27203c,0x8c0f2dc0);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);put(0x8c27203c,0x8c0f2dc8);
 put(0x8c1e3c10,1);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);
 put(0x8c272434,5);f=decode_state(r,MenuKind::Dialog);assert(f.kind==MenuKind::Dialog&&f.source_context==2&&f.row_count==0&&f.confirm_button==NativeLeft);
 put(0x8c272434,6);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);
 put(0x8c1b014c,0x00010106);put(0x8c2214c0,0x00000301);put(0x8c2214c8,0);put(0x8c2214de,0);
 assert(stock_picker_projection(r)); // Keep the Arcade art at its authored size.
 put(0x8c2214c0,0x00000101);assert(stock_picker_projection(r)); // Entry animation.
 put(0x8c2214c0,0x00000301);
 put(0x8c1b088c,0x4000);put(0x8c1b013c,0);put(0x8c271ffc,0);put(0x8c26fe1c,1);put(0x8c26fe18,0);put(0x8c1d54ac,0x8c272360);put(0x8c272368,0);
 ram[0x1b015c]=255;put(0x8c272328,0);put(0x8c1b0860,0);
 f=decode_state(r,MenuKind::ModeSelect);assert(f.kind==MenuKind::ModeSelect&&f.row==0&&f.aim_verified&&f.aim_gain_yaw==-12.5&&f.aim_gain_pitch==-12.5&&f.available_regions==31);
 put(0x8c2214d6,0xabcdef);f=decode_state(r,MenuKind::ModeSelect);assert(f.native_yaw==0&&f.native_pitch==0); // never use display-warped counters
 put(0x8c2214c8,1);assert(decode_state(r,MenuKind::ModeSelect).kind==MenuKind::None);put(0x8c2214c8,0);
 // The Story route uses the same picker but an independent owner and phase.
 put(0x8c1b014c,0x00000506);put(0x8c2214c0,0x00000401);
 put(0x8c2214c8,1);put(0x8c1b088c,0x00404000);put(0x8c2214de,2);
 assert(stock_picker_projection(r)); // Story branch owns the same native projection.
 f=decode_state(r,MenuKind::ModeSelect);
 assert(f.kind==MenuKind::ModeSelect&&f.mode_type==1&&f.interactive&&f.aim_verified);
 assert(f.source_context==0x101&&f.row_count==3&&f.row==2&&f.confirm_button==NativeLeft&&f.back_button==0);
 auto route=mode_goal(1,2);
 assert(route.valid&&route.yaw< -1790&&!mode_position_ack(f,route));
 f.native_yaw=route.yaw;assert(mode_position_ack(f,route));
 put(0x8c1b088c,0x4000);assert(decode_state(r,MenuKind::ModeSelect).kind==MenuKind::None);
 assert(!stock_picker_projection(r));
 put(0x8c2214c8,0);assert(!stock_picker_projection(r)); // Gameplay phase.
 // The Woods/Parachute selector has its own owner and two native yaw regions.
 put(0x8c1b014c,0x01070406);put(0x8c1b0884,4);put(0x8c2214c8,4);
 put(0x8c1b088c,0x4000);put(0x8c2214de,0);
 f=decode_state(r,MenuKind::ModeSelect);
 assert(stock_picker_projection(r)&&f.mode_type==4&&f.interactive&&f.aim_verified);
 assert(f.row_count==2&&f.available_regions==3&&f.source_context==InvasionRouteContext&&!f.back_button);
 for(int row=0;row<2;++row){
  const auto goal=mode_goal(4,row);f.row=row;f.native_yaw=goal.yaw;f.native_pitch=0;
  assert(goal.valid&&mode_position_ack(f,goal));
 }
 put(0x8c1b0884,3);assert(!stock_picker_projection(r));
 put(0x8c1b0884,4);put(0x8c1b088c,0);assert(!stock_picker_projection(r));
 // Outdoor Stage Select has its own live owner, nine cards and native progress bits.
 put(0x8c1b014c,0x01070208);put(0x8c2214c0,0x00000401);
 put(0x8c2214c8,6);put(0x8c2214cc,10);put(0x8c1b088c,0x00107002);
 put(0x8c2214de,0);put(0x8c271970,1);
 f=decode_state(r,MenuKind::ModeSelect);
 assert(stock_picker_projection(r)&&f.mode_type==6&&f.interactive&&f.aim_verified);
 assert(f.row_count==9&&f.available_regions==1&&f.confirm_button==NativeLeft);
 auto stage=mode_goal(6,0);
 assert(stage.valid&&stage.yaw>1568&&stage.pitch>0);
 f.native_yaw=stage.yaw;f.native_pitch=stage.pitch;assert(mode_position_ack(f,stage));
 put(0x8c271970,4);assert(decode_state(r,MenuKind::ModeSelect).available_regions==7);
 put(0x8c1b014c,0x01080208);assert(!stock_picker_projection(r));
 put(0x8c1b014c,0x00010106);put(0x8c2214c0,0x00000301);put(0x8c2214c8,0);put(0x8c2214de,0);
 put(0x8c1b088c,0);assert(!decode_state(r,MenuKind::ModeSelect).aim_verified);
 assert(!read_source(ram.data(),ram.size(),"OTHER").verified);
 // Actual023: three absent-VMU informational dialogs have distinct owners,
 // even though Save/Load share phase3 and all use the shared272430 phase.
 put(0x8c1b014c,0x00000504);put(0x8c272004,4);put(0x8c272030,1);
 put(0x8c272298,2);put(0x8c26fe14,1);put(0x8c1e3c10,0);
 for(unsigned row=1;row<=3;++row){
  put(0x8c272018,row);put(0x8c272430,row==3?2:3);
  const auto operation=row==1?0x8c27243c:row==2?0x8c272440:0x8c272448;
  put(operation,7);f=decode_state(r,MenuKind::Dialog);
  assert(f.kind==MenuKind::Dialog&&f.source_context==row+2&&f.row==-1&&f.row_count==0&&
    f.interactive&&f.context_active&&f.confirm_button==NativeLeft&&!f.back_button);
  put(operation,6);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);put(operation,7);
  put(0x8c1e3c10,1);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);put(0x8c1e3c10,0);
  put(0x8c272298,3);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);put(0x8c272298,2);
  put(0x8c272430,13);assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);
 }
 // A stale boot-photo phase cannot authorize an unrelated Options modal.
 put(0x8c272004,0);put(0x8c272434,5);
 assert(decode_state(r,MenuKind::Dialog).kind==MenuKind::None);
}
