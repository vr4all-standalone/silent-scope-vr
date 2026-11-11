// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_menu_aim.h"
#include <cassert>
#include <limits>
using namespace silent_scope::dc_menu;
int main(){
 assert(mode_region(0,0)==0&&mode_region(0,2500)==0&&mode_region(0,2501)==1);
 assert(mode_region(1121,-2500)==2&&mode_region(1120,-2500)==3&&mode_region(-1120,-2500)==4);
 for(int row=0;row<5;++row){auto g=mode_goal(0,row,row==1?0:-1);assert(g.valid&&mode_region(g.yaw,g.pitch)==row);}
 assert(!mode_goal(2,0,-1).valid&&!mode_goal(0,5,-1).valid&&!mode_goal(0,1,-1).valid);
 for(int row=0;row<3;++row){auto g=mode_goal(1,row);assert(g.valid&&story_route_region(g.yaw)==row);}
 assert(!mode_goal(1,3).valid&&!mode_goal(1,0,0).valid);
 auto indoor=mode_goal(0,1,0),outdoor=mode_goal(0,1,1);assert(indoor.yaw>0&&outdoor.yaw<0&&indoor.pitch==3500);
 MenuFacts f;f.kind=MenuKind::ModeSelect;f.verified=f.interactive=f.aim_verified=true;f.context_revision=7;f.mode_type=0;f.row_count=5;f.column_count=2;f.available_regions=31;
 f.aim_gain_yaw=f.aim_gain_pitch=-12.5;f.row=0;
 AimServo servo;auto g=mode_goal(0,4,-1);auto o=servo.poll(1,40,1,f,g);assert(o.dx==16&&o.dy==16&&o.waiting&&!o.fault);
 o=servo.poll(1.01,40.01,2,f,g);assert(o.dx==0&&o.dy==0&&o.waiting);
 f.native_yaw=f.native_pitch=-200;o=servo.poll(1.04,40.04,3,f,g);assert(o.dx>0&&o.dy>0&&!o.fault);
 const int dx=o.dx,dy=o.dy;f.native_yaw+=dx*-12.5;f.native_pitch+=dy*-12.5;
 o=servo.poll(1.08,40.08,4,f,g);assert(!o.fault);
 // A wrong-direction ACK fails closed and remains latched until explicit reset.
 servo.reset();f.native_yaw=f.native_pitch=0;g=mode_goal(0,1,0);o=servo.poll(2,41,5,f,g);assert(o.dy<0);
 f.native_pitch=-10;o=servo.poll(2.04,41.04,6,f,g);assert(o.fault&&o.dx==0&&o.dy==0);
 f.native_pitch=0;o=servo.poll(2.08,41.08,7,f,g);assert(o.fault);
 servo.reset();o=servo.poll(3,42,8,f,g);assert(o.waiting);o=servo.poll(3.31,42.31,9,f,g);assert(o.fault);
 servo.reset();f.aim_verified=false;o=servo.poll(4,43,10,f,g);assert(o.fault&&o.dx==0&&o.dy==0);
 servo.reset();f.aim_verified=true;f.native_pitch=std::numeric_limits<double>::quiet_NaN();o=servo.poll(4,43,10,f,g);assert(o.fault);
 servo.reset();f.native_pitch=0;g.yaw=std::numeric_limits<double>::quiet_NaN();o=servo.poll(4,43,10,f,g);assert(o.fault&&o.dx==0&&o.dy==0);
 // The menu policy freezes its native region/cell and confirms only actual ACK.
 Policy p;PolicyInput i;i.host_seconds=5;i.native_seconds=44;f.native_pitch=f.native_yaw=0;i.facts=f;p.poll(i);
 i.fresh_click=i.trigger_down=true;i.clicked.kind=MenuKind::ModeSelect;i.clicked.verb=MenuVerb::Confirm;i.clicked.context_revision=7;i.clicked.row=1;i.clicked.column=1;i.click_host_seconds=5;
 auto r=p.poll(i);assert(r.aim_goal.valid&&r.aim_goal.yaw<0&&r.buttons==0);
 i.fresh_click=false;i.hover={};i.facts.native_yaw=outdoor.yaw;i.facts.native_pitch=outdoor.pitch;i.facts.row=0;i.host_seconds=5.04;i.native_seconds=44.04;
 r=p.poll(i);assert(r.buttons==0); // raw center alone is not native selection acknowledgement
 i.facts.row=1;i.facts.column=1;i.facts.confirm_button=NativeLeft;i.facts.native_yaw+=5;i.facts.native_pitch-=5;r=p.poll(i);assert(r.buttons==NativeLeft);
 i.facts.native_yaw=-900;i.host_seconds=5.06;i.native_seconds=44.06;r=p.poll(i);assert(r.buttons==0); // same region is insufficient after unowned cursor movement
 Policy back;i.trigger_down=i.fresh_click=false;i.host_seconds=6;i.native_seconds=45;i.facts.back_button=NativeRight;back.poll(i);
 i.trigger_down=i.fresh_click=true;i.clicked.verb=MenuVerb::Back;i.clicked.row=i.clicked.column=-1;i.click_host_seconds=6;r=back.poll(i);assert(r.buttons==NativeRight);
 i.fresh_click=false;i.host_seconds=6.05;i.native_seconds=45.05;r=back.poll(i);assert(r.buttons==NativeRight); // Back is not a positional shot
 f.mode_type=1;f.row_count=3;f.column_count=0;f.available_regions=7;f.row=1;f.column=-1;
 f.confirm_button=NativeLeft;f.native_yaw=f.native_pitch=0;f.context_revision=8;
 Policy branch;i={};i.host_seconds=7;i.native_seconds=46;i.facts=f;branch.poll(i);
 i.fresh_click=i.trigger_down=true;i.clicked={MenuKind::ModeSelect,MenuVerb::Confirm,2,-1,-1,0,8};
 i.click_host_seconds=7;r=branch.poll(i);assert(r.aim_goal.valid&&r.buttons==0);
 servo.reset();o=servo.poll(7,46,11,f,r.aim_goal);assert(o.dx>0&&o.dy==0&&!o.fault);
 i.fresh_click=false;i.host_seconds+=.1;i.native_seconds+=.1;i.facts.native_yaw=-3000;
 r=branch.poll(i);assert(r.buttons==0); // The guest still owns the actual route selection.
 i.facts.row=2;r=branch.poll(i);assert(r.fresh_confirm&&r.buttons==NativeLeft);
 i.facts.context_active=false;r=branch.poll(i);assert(r.buttons==0);
}
