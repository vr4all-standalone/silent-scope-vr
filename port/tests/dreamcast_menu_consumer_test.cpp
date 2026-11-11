// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_mouse_input.h"
#include "dc_menu_controls.h"
#include "dc_menu_presentation.h"
#include <cassert>
using namespace silent_scope;
int main(){
 dc_mouse::Consumer c;dc_xr::InputBridge bridge;bridge.set_sink(c.sink());
 dc_xr::Target t;t.focused=t.tracked=t.native_ui=t.point_valid=true;t.native_point={0,0,-1};t.hand=1;t.host_seconds=1;
 t.ui_action.kind=dc_menu::MenuKind::Training;t.ui_action.verb=dc_menu::MenuVerb::Focus;t.ui_action.row=0;t.ui_action.column=0;t.ui_action.context_revision=9;
 dc_mouse::Feedback f;f.code_verified=f.mouse_device=true;f.generation=2;f.poll_sequence=1;f.native_seconds=40;
 f.menu.kind=dc_menu::MenuKind::Training;f.menu.verified=f.menu.interactive=true;f.menu.context_revision=9;f.menu.row=f.menu.column=0;f.menu.row_count=4;f.menu.column_count=3;f.menu.confirm_button=dc_menu::NativeLeft;
 bridge.submit(t);c.poll(1,f);t.host_seconds=1.01;bridge.submit(t);++f.poll_sequence;f.native_seconds+=.01;c.poll(1.01,f);
 // Capture the desired pair at physical click, then move hover before polling.
 t.host_seconds=1.02;t.ui_select=true;t.ui_action.verb=dc_menu::MenuVerb::Confirm;t.ui_action.row=2;t.ui_action.column=1;bridge.submit(t);
 t.host_seconds=1.025;t.ui_select=false;t.ui_action.verb=dc_menu::MenuVerb::Focus;t.ui_action.row=0;t.ui_action.column=0;bridge.submit(t);
 ++f.poll_sequence;f.native_seconds+=.02;auto p=c.poll(1.03,f);assert(p.wheel==1&&p.pressed==0);
 f.menu.row=1;++f.poll_sequence;f.native_seconds+=.02;p=c.poll(1.05,f);assert(p.wheel==1&&p.pressed==0);
 f.menu.row=2;++f.poll_sequence;f.native_seconds+=.02;p=c.poll(1.07,f);assert(p.dx==16&&p.pressed==0);
 f.menu.column=1;++f.poll_sequence;f.native_seconds+=.02;p=c.poll(1.09,f);assert((p.pressed&dc_mouse::Left)!=0);
 // Duplicate poll cannot keep native Left after its host deadline.
 t.host_seconds=1.36;bridge.submit(t);p=c.poll(1.36,f);assert(p.pressed==0);
 // Losing proof suppresses all generic UI fallback, even with a fresh trigger.
 f.menu.verified=false;++f.poll_sequence;t.host_seconds=1.37;t.ui_select=true;bridge.submit(t);p=c.poll(1.37,f);assert(p.pressed==0&&p.dx==0&&p.dy==0&&p.wheel==0);
 // Native-known menus cannot fall back to generic Left or gameplay fire when
 // their current frame has no positive geometry/hit action.
 dc_mouse::Consumer guarded;dc_xr::InputBridge gb;gb.set_sink(guarded.sink());
 auto u=t;u.ui_action={};u.ui_select=false;u.host_seconds=2;f.menu.verified=true;f.poll_sequence=20;f.native_seconds=41;gb.submit(u);guarded.poll(2,f);
 u.host_seconds=2.01;gb.submit(u);++f.poll_sequence;guarded.poll(2.01,f);u.ui_select=true;u.host_seconds=2.02;gb.submit(u);++f.poll_sequence;p=guarded.poll(2.02,f);assert(p.pressed==0);
 u.native_ui=false;u.ui_select=false;u.fire=false;u.host_seconds=2.03;gb.submit(u);++f.poll_sequence;guarded.poll(2.03,f);
 u.fire=true;u.host_seconds=2.04;gb.submit(u);++f.poll_sequence;p=guarded.poll(2.04,f);assert(p.pressed==0&&p.dx==0&&p.dy==0);
 // A verified selector can remain in native memory after it loses ownership.
 // Resume both relative aim and Left on a fresh gameplay trigger.
 f.menu.context_active=f.menu.interactive=false;
 f.gain={true,123,-2.5,-2.5};
 u.native_direction={.01,0,-std::sqrt(1.-.01*.01)};
 u.direction_valid=true;
 u.point_valid=false;
 u.fire=false;u.host_seconds=2.05;gb.submit(u);++f.poll_sequence;
 p=guarded.poll(2.05,f);assert(p.dx>0&&p.pressed==0);
 u.fire=true;u.host_seconds=2.06;gb.submit(u);++f.poll_sequence;
 p=guarded.poll(2.06,f);assert(p.fresh_fire&&(p.pressed&dc_mouse::Left));
 f.menu.context_active=f.menu.interactive=true;
 // Keep the physical trigger level off-panel. Entering a typed Confirm while
 // still holding it is not a new gesture and must not invent native Left.
 dc_mouse::Consumer off;dc_xr::InputBridge ob;ob.set_sink(off.sink());auto h=t;
 h.ui_select=false;h.host_seconds=3;h.ui_action.kind=dc_menu::MenuKind::Training;h.ui_action.verb=dc_menu::MenuVerb::Confirm;
 h.ui_action.row=f.menu.row;h.ui_action.column=f.menu.column;f.poll_sequence=30;f.native_seconds=42;
 ob.submit(h);off.poll(3,f);h.host_seconds=3.01;ob.submit(h);++f.poll_sequence;off.poll(3.01,f);
 h.point_valid=false;h.ui_select=true;h.host_seconds=3.02;ob.submit(h);++f.poll_sequence;p=off.poll(3.02,f);assert(p.pressed==0);
 h.point_valid=true;h.host_seconds=3.03;ob.submit(h);++f.poll_sequence;p=off.poll(3.03,f);assert(p.pressed==0);
 h.ui_select=false;h.host_seconds=3.04;ob.submit(h);++f.poll_sequence;off.poll(3.04,f);
 h.ui_select=true;h.host_seconds=3.05;ob.submit(h);++f.poll_sequence;p=off.poll(3.05,f);assert((p.pressed&dc_mouse::Left)!=0);
 // Mode Select has a separate unscoped native gain and raw-angle servo.
 dc_mouse::Consumer mode;dc_xr::InputBridge mb;mb.set_sink(mode.sink());dc_mouse::Feedback mf;
 mf.code_verified=mf.mouse_device=true;mf.generation=5;mf.poll_sequence=1;mf.native_seconds=50;mf.gain.signature=700;
 auto& mm=mf.menu;mm.kind=dc_menu::MenuKind::ModeSelect;mm.verified=mm.interactive=mm.aim_verified=true;mm.context_revision=12;mm.mode_type=0;mm.row=0;mm.row_count=5;mm.column_count=2;mm.available_regions=31;
 mm.aim_gain_yaw=mm.aim_gain_pitch=-12.5;mm.aim_profile=1;mm.confirm_button=dc_menu::NativeLeft;
 mm.source_context=dc_menu::ArcadeModeContext;mm.native_seconds=50;
 dc_menu_controls::Controller cards;
 cards.begin(mm,true,-1,dc_menu::selector_panel(mm,true,49.95));
 auto mt=h;mt.host_seconds=5;mt.ui_select=false;mt.ui_action={};mt.ui_action.kind=mm.kind;mt.ui_action.context_revision=12;mb.submit(mt);mode.poll(5,mf);
 mt.host_seconds=5.01;mb.submit(mt);++mf.poll_sequence;mode.poll(5.01,mf);
 mt.host_seconds=5.02;mt.ui_select=true;mt.ui_action=cards.resolve({},-1,0,405,105);
 assert(mt.ui_action.verb==dc_menu::MenuVerb::Focus&&mt.ui_action.row==1&&mt.ui_action.column==1);
 mt.ui_action.verb=dc_menu::MenuVerb::Confirm;mb.submit(mt);++mf.poll_sequence;
 p=mode.poll(5.02,mf);assert(p.dx==16&&p.dy==-16&&p.pressed==0);
 const auto first_motion=p;mm.navigation_neutral=false;mt.host_seconds+=.01;mb.submit(mt);++mf.poll_sequence;mf.native_seconds+=.01;p=mode.poll(mt.host_seconds,mf);assert(p.dx==0&&p.dy==0);
 p=first_motion;
 for(int step=0;step<4;++step){
  mm.native_yaw+=p.dx*-12.5;mm.native_pitch+=p.dy*-12.5;mm.row=dc_menu::mode_region(mm.native_yaw,mm.native_pitch);mm.column=mm.row==1?1:-1;
  mt.host_seconds+=.04;mb.submit(mt);mf.native_seconds+=.04;++mf.poll_sequence;++mf.gain.signature; // irrelevant gameplay hint changes
  p=mode.poll(mt.host_seconds,mf);
  if(step==0){assert(p.dx==64&&p.dy==-128);mm.navigation_neutral=true;}
 }
 assert(mm.native_yaw==-1000&&mm.native_pitch==3500&&(p.pressed&dc_mouse::Left)!=0);
 mm.aim_verified=false;mt.host_seconds+=.02;mb.submit(mt);mf.native_seconds+=.02;++mf.poll_sequence;p=mode.poll(mt.host_seconds,mf);assert((p.pressed&dc_mouse::Left)!=0); // held button does not request movement
 ++mm.aim_profile;mt.host_seconds+=.02;mb.submit(mt);mf.native_seconds+=.02;++mf.poll_sequence;p=mode.poll(mt.host_seconds,mf);assert(p.pressed==0);
}
