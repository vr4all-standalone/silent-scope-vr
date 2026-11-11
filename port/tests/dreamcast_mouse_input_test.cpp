#include <iostream>
#include <cmath>
#include <limits>
#if __has_include("dreamcast/dc_mouse_input.h")
#include "dreamcast/dc_mouse_input.h"
#define HAS_DC_MOUSE 1
#endif
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
#ifdef HAS_DC_MOUSE
using namespace silent_scope::dc_mouse;
silent_scope::dc_xr::Target target(double time,double yaw=0,double pitch=0){
 silent_scope::dc_xr::Target t;t.focused=t.tracked=t.direction_valid=true;t.native_direction.z=-1;
 t.host_seconds=time;t.yaw_units=yaw;t.pitch_units=pitch;t.hand=1;t.epoch=1;return t;
}
Feedback feedback(unsigned sequence,double time,double yaw=0,double pitch=0){
 Feedback f;f.code_verified=f.mouse_device=true;f.generation=1;f.poll_sequence=sequence;f.native_seconds=time;
 f.yaw=yaw;f.pitch=pitch;f.gain={true,1,-2.5,-2.5};return f;
}
#endif
int main(){
#ifndef HAS_DC_MOUSE
 REQUIRE(false&&"Native mouse consumer missing");
#else
 SourceContext c;c.code_verified=c.mouse_override=true;c.scope_branch=1;c.input_sign=-1;c.speed_index=1;
 auto hint=source_hint(c);REQUIRE(hint.verified&&hint.yaw==-2.5&&hint.pitch==-2.5);
 c.speed_index=2;REQUIRE(source_hint(c).yaw==-5);c.reverse_y=true;REQUIRE(source_hint(c).pitch==5);
 c.mode=2;REQUIRE(!source_hint(c).verified);
 Consumer mouse;REQUIRE(mouse.poll(0,feedback(1,0)).pressed==0);
 mouse.submit(target(0));auto p=mouse.poll(0,feedback(2,0));REQUIRE(p.dx==0&&p.dy==0&&!p.pressed);
 auto t=target(.01,-40);t.fire=true;mouse.submit(t);p=mouse.poll(.01,feedback(3,.01));
 REQUIRE(p.dx==16&&p.dy==0&&(p.pressed&Left)&&p.fresh_fire&&p.status==Status::WaitingAck);
 p=mouse.poll(.02,feedback(4,.02,-20));REQUIRE(p.dx==0&&(p.pressed&Left)); // Aim acknowledgement remains independent of held button.
 p=mouse.poll(.03,feedback(5,.03,-40));REQUIRE(p.dx==0&&(p.pressed&Left));
 p=mouse.poll(.04,feedback(6,.04,-40));REQUIRE((p.pressed&Left)&&p.learned_yaw&&p.yaw_gain==-2.5);
 mouse.submit(target(.05,-40));p=mouse.poll(.05,feedback(7,.05,-40));REQUIRE(!p.pressed);
 t=target(.06,-200);mouse.submit(t);p=mouse.poll(.06,feedback(8,.06,-40));REQUIRE(p.dx==64);
 p=mouse.poll(.07,feedback(8,.06,-40));REQUIRE(p.dx==0); // Duplicate native poll never repeats a delta.
 mouse.submit(target(.08,-200));p=mouse.poll(.08,feedback(9,.08,-200));REQUIRE(p.dx==0&&p.status==Status::Ready);
 t=target(.09,-6000);t.fire=true;mouse.submit(t);p=mouse.poll(.09,feedback(10,.09,-200));
 REQUIRE(p.status==Status::Unreachable&&!p.pressed&&!p.dx);
 t=target(.10,-200);t.fire=true;mouse.submit(t);p=mouse.poll(.10,feedback(11,.10,-200));REQUIRE(!p.pressed);
 mouse.submit(target(.11,-200));mouse.poll(.11,feedback(12,.11,-200));
 t=target(.12,-200);t.fire=true;mouse.submit(t);REQUIRE(mouse.poll(.12,feedback(13,.12,-200)).pressed&Left);
 REQUIRE(mouse.poll(.125,feedback(13,.12,-200)).pressed&Left); // Duplicate read preserves held bits without a new edge.
 p=mouse.poll(.5,feedback(14,.5,-200));REQUIRE(p.status==Status::StaleTarget&&!p.pressed);
 t=target(.51,-200);t.fire=true;mouse.submit(t);REQUIRE(!mouse.poll(.51,feedback(15,.51,-200)).pressed);
 // A focus loss/restoration entirely between native polls still invalidates the hold.
 mouse.submit(target(.52,-200));mouse.poll(.52,feedback(16,.52,-200));
 t=target(.53,-200);t.fire=true;mouse.submit(t);REQUIRE(mouse.poll(.53,feedback(17,.53,-200)).pressed&Left);
 t.focused=false;t.host_seconds=.54;mouse.submit(t);t.focused=true;t.host_seconds=.55;mouse.submit(t);
 REQUIRE(!mouse.poll(.55,feedback(18,.55,-200)).pressed);
 // A source signature/generation change cancels motion and requires a fresh trigger.
 mouse.submit(target(.56,-200));mouse.poll(.56,feedback(19,.56,-200));
 t=target(.57,-400);t.fire=true;mouse.submit(t);REQUIRE(mouse.poll(.57,feedback(20,.57,-200)).dx!=0);
 auto f=feedback(21,.58,-220);f.gain.signature=2;REQUIRE(!mouse.poll(.58,f).pressed);
 // Native UI clicks have no gameplay movement and explicitly unverified pointing.
 t=target(.60);t.native_ui=true;t.point_valid=true;t.native_point.z=-1;mouse.submit(t);mouse.poll(.60,feedback(22,.60));
 t.host_seconds=.61;t.ui_select=true;mouse.submit(t);p=mouse.poll(.61,feedback(23,.61));
 REQUIRE((p.pressed&Left)&&!p.dx&&!p.dy&&p.ui_pointing_unverified);
 t=target(.62);mouse.submit(t);mouse.poll(.62,feedback(24,.62));
 t.host_seconds=.63;t.start=true;mouse.submit(t);REQUIRE(mouse.poll(.63,feedback(25,.63)).pressed&Middle);
 t.host_seconds=.75;mouse.submit(t);REQUIRE(!(mouse.poll(.75,feedback(26,.75)).pressed&Middle)); // No repeat Start while held.
 // Unsupported branches, nonfinite targets, reverse clocks and bounds are explicit.
 t=target(.80,-100);mouse.submit(t);f=feedback(27,.80);f.gain.verified=false;
 p=mouse.poll(.80,f);REQUIRE(p.status==Status::UnsupportedGain&&!p.dx&&!p.pressed);
 t.yaw_units=std::numeric_limits<double>::quiet_NaN();t.host_seconds=.81;mouse.submit(t);
 REQUIRE(!mouse.poll(.81,feedback(28,.81)).pressed);
 Consumer timeout;timeout.submit(target(1));timeout.poll(1,feedback(1,1));
 timeout.submit(target(1.01,-1000));REQUIRE(timeout.poll(1.01,feedback(2,1.01)).dx==16);
 timeout.submit(target(1.27,-1000));p=timeout.poll(1.27,feedback(3,1.27));REQUIRE(p.status==Status::AckTimeout&&!p.dx&&!p.pressed);
 // Fresh neutral input plus a later valid feedback sample recovers a transient stall.
 timeout.submit(target(1.28,-1000));p=timeout.poll(1.28,feedback(4,1.28));
 REQUIRE(p.status==Status::Ready&&!p.dx&&!p.pressed);
 t=target(1.29,-1000);t.fire=true;timeout.submit(t);p=timeout.poll(1.29,feedback(5,1.29));
 REQUIRE(p.dx==16&&(p.pressed&Left)&&p.fresh_fire); // Recovered source accepts only this new physical edge.
 auto sink=timeout.sink();sink.reset(sink.user,10);REQUIRE(timeout.poll(1.30,feedback(6,1.30)).pressed==0);
 Consumer startup;auto no_aim=target(2);no_aim.direction_valid=false;startup.submit(no_aim);startup.poll(2,feedback(1,2));
 no_aim.host_seconds=2.01;no_aim.start=true;startup.submit(no_aim);
 REQUIRE(startup.poll(2.01,feedback(2,2.01)).pressed&Middle); // Explicit Start does not require an aim endpoint.
 no_aim.host_seconds=2.02;startup.submit(no_aim);
 REQUIRE(startup.poll(2.02,feedback(3,2.02)).pressed&Middle); // Aim rejection must not truncate the explicit native pulse.
 Consumer unsupported;unsupported.submit(target(2.1));f=feedback(1,2.1);f.gain.verified=false;unsupported.poll(2.1,f);
 t=target(2.11);t.start=true;unsupported.submit(t);f.poll_sequence=2;f.native_seconds=2.11;
 REQUIRE(unsupported.poll(2.11,f).pressed&Middle);
 t.host_seconds=2.12;unsupported.submit(t);f.poll_sequence=3;f.native_seconds=2.12;
 REQUIRE(unsupported.poll(2.12,f).pressed&Middle);
 for(int transition=0;transition<5;++transition){
  Consumer lifecycle;lifecycle.submit(target(4));lifecycle.poll(4,feedback(1,4));
  t=target(4.01);t.fire=true;lifecycle.submit(t);REQUIRE(lifecycle.poll(4.01,feedback(2,4.01)).pressed&Left);
  t.host_seconds=4.02;
  if(transition==0)t.hand=0;
  if(transition==1)t.tracked=false;
  if(transition==2)t.menu=true;
  if(transition==3)t.epoch=2;
  lifecycle.submit(t);f=feedback(3,4.02);if(transition==4)f.generation=2;
  p=lifecycle.poll(4.02,f);REQUIRE(!p.pressed&&!p.dx&&!p.dy);
 }
 Consumer limits;limits.submit(target(3));limits.poll(3,feedback(1,3));
 t=target(3.01,-40);limits.submit(t);p=limits.poll(3.01,feedback(2,3.01));REQUIRE(p.dx==16);
 limits.submit(target(3.02,-40));limits.poll(3.02,feedback(3,3.02,-40));
 limits.submit(target(3.03,-40));limits.poll(3.03,feedback(4,3.03,-40));
 limits.submit(target(3.04,-5000));p=limits.poll(3.04,feedback(5,3.04,-40));REQUIRE(p.dx==511);
 // Real bridge->consumer path: native feedback acknowledges the previous
 // relative-mouse packet while physical aim keeps moving every XR sample.
 // A trigger is a native button event, not a requirement to stop the rifle.
 Consumer moving;silent_scope::dc_xr::InputBridge bridge;bridge.set_sink(moving.sink());
 double native_yaw=0;int delivered_edges=0;
 for(unsigned step=0;step<25;++step){
  const double now=10+step/30.;
  for(unsigned xr=0;xr<3;++xr){
   const double angle=(-40.*step-10.*xr)*6.2831853071795864769/65536.;
   auto sample=target(now-.02+xr*.01);sample.native_direction={-std::sin(angle),0,-std::cos(angle)};
   sample.fire=(step>=3&&step<=7)||(step>=13&&step<=17);bridge.submit(sample);
  }
  p=moving.poll(now,feedback(step+1,now,native_yaw));
  REQUIRE(p.status!=Status::AckTimeout&&p.status!=Status::UnsupportedGain);
  if(p.fresh_fire){++delivered_edges;REQUIRE(p.pressed&Left);}
  native_yaw+=p.dx*-2.5;
 }
 REQUIRE(delivered_edges==2);
 // A complete physical tap between native polls is latched as one native
 // packet, even though the latest XR sample already reports release.
 Consumer tapped;silent_scope::dc_xr::InputBridge tap_bridge;tap_bridge.set_sink(tapped.sink());
 auto tap=target(20);tap_bridge.submit(tap);tapped.poll(20,feedback(1,20));
 tap.host_seconds=20.01;tap.fire=true;tap_bridge.submit(tap);
 tap.host_seconds=20.02;tap.fire=false;tap_bridge.submit(tap);
 p=tapped.poll(20.03,feedback(2,20.03));REQUIRE(p.fresh_fire&&(p.pressed&Left));
 p=tapped.poll(20.04,feedback(3,20.04));REQUIRE(!p.fresh_fire&&!p.pressed);
 // A release/repress between polls must produce native Up before the next
 // fresh Left; metadata alone cannot pretend an already-held bit is an edge.
 tap.host_seconds=20.05;tap.fire=true;tap_bridge.submit(tap);
 p=tapped.poll(20.05,feedback(4,20.05));REQUIRE(p.fresh_fire&&(p.pressed&Left));
 tap.host_seconds=20.055;tap.fire=false;tap_bridge.submit(tap);
 tap.host_seconds=20.06;tap.fire=true;tap_bridge.submit(tap);
 p=tapped.poll(20.065,feedback(5,20.065));REQUIRE(!p.fresh_fire&&!p.pressed);
 p=tapped.poll(20.075,feedback(6,20.075));REQUIRE(p.fresh_fire&&(p.pressed&Left));
 // Latched taps must not survive safety transitions, source generations,
 // expiry, or a press outside the actual native caps that later moves inside.
 for(int reason=0;reason<9;++reason){
  Consumer guarded;silent_scope::dc_xr::InputBridge guard_bridge;guard_bridge.set_sink(guarded.sink());
  auto sample=target(30);guard_bridge.submit(sample);guarded.poll(30,feedback(1,30));
  sample.host_seconds=30.01;sample.fire=true;
  if(reason==7){const double angle=6000*6.2831853071795864769/65536.;sample.native_direction={-std::sin(angle),0,-std::cos(angle)};}
  if(reason==8)sample.native_direction.x=std::numeric_limits<double>::quiet_NaN();
  guard_bridge.submit(sample);
  sample.host_seconds=30.02;
  if(reason==0)sample.focused=false;
  if(reason==1)sample.tracked=false;
  if(reason==2)sample.menu=true;
  if(reason==3)sample.hand=0;
  if(reason==4)guard_bridge.invalidate();
  if(reason<5)guard_bridge.submit(sample);
  sample.host_seconds=reason==6?30.4:30.03;sample.fire=false;sample.native_direction={0,0,-1};guard_bridge.submit(sample);
  auto actual=feedback(2,sample.host_seconds);if(reason==5)actual.generation=2;
  p=guarded.poll(sample.host_seconds,actual);REQUIRE(!p.pressed&&!p.fresh_fire);
 }
 // The actual bridge/consumer delivers only verified nativeUI wheel notches.
 Consumer navigation;silent_scope::dc_xr::InputBridge nav_bridge;nav_bridge.set_sink(navigation.sink());
 auto nav=target(40);nav.native_ui=true;nav.direction_valid=false;auto nf=feedback(1,40);nf.menu_navigation_verified=true;
 nav_bridge.submit(nav);navigation.poll(40,nf);
 nav.host_seconds=40.01;nav.ui_navigation=1;nav_bridge.submit(nav);nf.poll_sequence=2;nf.native_seconds=40.01;
 p=navigation.poll(40.01,nf);REQUIRE(p.wheel==1&&!p.dx&&!p.dy&&!p.pressed);
 p=navigation.poll(40.011,nf);REQUIRE(!p.wheel); // Duplicate Maple read never repeats a notch.
 nav.host_seconds=40.35;nav_bridge.submit(nav);nf.poll_sequence=3;nf.native_seconds=40.35;
 REQUIRE(!navigation.poll(40.35,nf).wheel);
 nav.host_seconds=40.361;nav_bridge.submit(nav);nf.poll_sequence=4;nf.native_seconds=40.361;
 REQUIRE(navigation.poll(40.361,nf).wheel==1);
 nav.host_seconds=40.512;nav_bridge.submit(nav);nf.poll_sequence=5;nf.native_seconds=40.512;
 REQUIRE(navigation.poll(40.512,nf).wheel==1);
 nav.host_seconds=40.52;nav.ui_navigation=0;nav_bridge.submit(nav);
 nav.host_seconds=40.53;nav.ui_navigation=-1;nav_bridge.submit(nav);
 nav.host_seconds=40.54;nav.ui_navigation=0;nav_bridge.submit(nav);
 nf.poll_sequence=6;nf.native_seconds=40.55;REQUIRE(navigation.poll(40.55,nf).wheel==-1);
 nf.poll_sequence=7;nf.native_seconds=40.56;REQUIRE(!navigation.poll(40.56,nf).wheel);
 for(int reason=0;reason<9;++reason){
  Consumer safe;silent_scope::dc_xr::InputBridge b;b.set_sink(safe.sink());
  auto q=target(50);q.native_ui=true;auto g=feedback(1,50);g.menu_navigation_verified=true;b.submit(q);safe.poll(50,g);
  q.host_seconds=50.01;q.ui_navigation=1;b.submit(q);g.poll_sequence=2;g.native_seconds=50.01;
  REQUIRE(safe.poll(50.01,g).wheel==1);
  q.host_seconds=50.02;
  if(reason==0)q.focused=false;
  if(reason==1)q.tracked=false;
  if(reason==2)q.menu=true;
  if(reason==3)q.hand=0;
  if(reason==4)q.native_ui=false;
  if(reason==5)b.invalidate();
  b.submit(q);g.poll_sequence=3;g.native_seconds=50.02;
  if(reason==6)g.generation=2;
  if(reason==7)g.menu_navigation_verified=false;
  if(reason==8)g.native_seconds=50.3;
  REQUIRE(!safe.poll(reason==8?50.3:50.02,g).wheel);
  q.host_seconds=50.4;q.focused=q.tracked=q.native_ui=true;q.menu=false;b.submit(q);
  g.poll_sequence=4;g.native_seconds=50.4;g.menu_navigation_verified=true;
  REQUIRE(!safe.poll(50.4,g).wheel); // Held stick cannot restart repeat after restoration.
 }
 Consumer pointer;silent_scope::dc_xr::InputBridge pb;pb.set_sink(pointer.sink());
 auto pt=target(60);pt.native_ui=pt.ui_main_menu=pt.point_valid=true;pt.native_point={0,0,-1};pt.ui_pointer_item=-1;
 auto pf=feedback(1,60);pf.menu_navigation_verified=pf.main_menu_verified=true;pf.menu_count=4;pf.menu_selected=0;
 pb.submit(pt);pointer.poll(60,pf);
 pt.host_seconds=60.01;pt.ui_pointer_item=3;pt.ui_select=true;pb.submit(pt);
 pt.host_seconds=60.02;pt.ui_pointer_item=0;pt.ui_select=false;pb.submit(pt); // Pointer moved/released before native poll.
 pf.poll_sequence=2;pf.native_seconds=60.03;p=pointer.poll(60.03,pf);
 REQUIRE(p.wheel==1&&!p.pressed&&p.menu_target==3);
 for(int step=1;step<=3;++step){
  pt.host_seconds=60.03+step*.08-.03;pb.submit(pt);++pf.poll_sequence;pf.native_seconds=pt.host_seconds;pf.menu_selected=step;pf.menu_native_wheel=1;
  p=pointer.poll(pt.host_seconds,pf);REQUIRE(!p.wheel&&!p.pressed);
  pt.host_seconds+=.03;pb.submit(pt);++pf.poll_sequence;pf.native_seconds=pt.host_seconds;pf.menu_native_wheel=0;
  p=pointer.poll(pt.host_seconds,pf);REQUIRE(step==3?((p.pressed&Left)&&!p.wheel):(p.wheel==1&&!p.pressed));
 }
 pt.host_seconds=60.3;pb.submit(pt);++pf.poll_sequence;pf.native_seconds=60.3;
 p=pointer.poll(60.3,pf);REQUIRE(!p.wheel&&(p.pressed&Left)); // Captured hover0 cannot undo highlight while native confirms3.
 pt.host_seconds=60.39;pb.submit(pt);++pf.poll_sequence;pf.native_seconds=60.39;REQUIRE(!pointer.poll(60.39,pf).pressed);
 // A trigger pulled on blank/off-panel cannot become a fresh item click by moving into it while held.
 Consumer blank;silent_scope::dc_xr::InputBridge bb;bb.set_sink(blank.sink());
 auto bt=pt;bt.host_seconds=65;bt.ui_select=false;bt.ui_pointer_item=-1;auto bf=pf;bf.poll_sequence=1;bf.native_seconds=65;bf.menu_selected=0;
 bb.submit(bt);blank.poll(65,bf);
 bt.host_seconds=65.01;bt.point_valid=false;bt.ui_select=true;bb.submit(bt);
 bt.host_seconds=65.02;bt.point_valid=true;bt.ui_pointer_item=0;bb.submit(bt);bf.poll_sequence=2;bf.native_seconds=65.03;
 REQUIRE(!blank.poll(65.03,bf).pressed);
 for(int reason=0;reason<8;++reason){
  Consumer safe;silent_scope::dc_xr::InputBridge b;b.set_sink(safe.sink());auto q=pt;q.host_seconds=70;q.ui_pointer_item=-1;
  auto g=pf;g.poll_sequence=1;g.native_seconds=70;g.menu_selected=0;b.submit(q);safe.poll(70,g);
  q.host_seconds=70.01;q.ui_pointer_item=3;q.ui_select=true;b.submit(q);g.poll_sequence=2;g.native_seconds=70.01;
  REQUIRE(safe.poll(70.01,g).wheel==1);
  q.host_seconds=70.02;
  if(reason==0)q.focused=false;
  if(reason==1)q.tracked=false;
  if(reason==2)q.menu=true;
  if(reason==3)q.hand=0;
  if(reason==4)q.ui_main_menu=false;
  if(reason==5)b.invalidate();
  b.submit(q);g.poll_sequence=3;g.native_seconds=70.02;if(reason==6)g.generation=2;if(reason==7)g.main_menu_verified=false;
  REQUIRE(!safe.poll(70.02,g).pressed);
  q.host_seconds=70.1;q.focused=q.tracked=q.ui_main_menu=true;q.menu=false;b.submit(q);g.poll_sequence=4;g.native_seconds=70.1;g.main_menu_verified=true;g.menu_selected=3;
  REQUIRE(!safe.poll(70.1,g).pressed);
 }
 // Consumer validates a genuine release/repress between native polls even
 // when MenuPointer never observed a released native-polled target level.
 Consumer retry;silent_scope::dc_xr::InputBridge rb;rb.set_sink(retry.sink());
 auto rt=pt;rt.host_seconds=80;rt.ui_pointer_item=0;rt.ui_select=false;auto rf=pf;rf.poll_sequence=1;rf.native_seconds=80;rf.menu_selected=0;
 rb.submit(rt);retry.poll(80,rf);rt.host_seconds=80.01;rt.ui_select=true;rb.submit(rt);rf.poll_sequence=2;rf.native_seconds=80.01;
 REQUIRE(retry.poll(80.01,rf).pressed&Left);
 rt.host_seconds=80.12;rb.submit(rt);rf.poll_sequence=3;rf.native_seconds=80.12;REQUIRE(!retry.poll(80.12,rf).pressed);
 rt.host_seconds=80.125;rt.ui_select=false;rb.submit(rt);rt.host_seconds=80.13;rt.ui_select=true;rb.submit(rt);
 rf.poll_sequence=4;rf.native_seconds=80.14;REQUIRE(retry.poll(80.14,rf).pressed&Left);
 Consumer immediate;silent_scope::dc_xr::InputBridge ib;ib.set_sink(immediate.sink());
 rt.host_seconds=90;rt.ui_select=false;rf.poll_sequence=1;rf.native_seconds=90;ib.submit(rt);immediate.poll(90,rf);
 rt.host_seconds=90.01;rt.ui_select=true;ib.submit(rt);rf.poll_sequence=2;rf.native_seconds=90.01;REQUIRE(immediate.poll(90.01,rf).pressed&Left);
 rt.host_seconds=90.015;rt.ui_select=false;ib.submit(rt);rt.host_seconds=90.016;rt.ui_select=true;ib.submit(rt);
 rf.poll_sequence=3;rf.native_seconds=90.02;REQUIRE(immediate.poll(90.02,rf).pressed&Left); // Initial hold completes before retry.
 rt.host_seconds=90.12;ib.submit(rt);rf.poll_sequence=4;rf.native_seconds=90.12;REQUIRE(!immediate.poll(90.12,rf).pressed); // Actual native Up required.
 rf.poll_sequence=5;rf.native_seconds=90.13;REQUIRE(immediate.poll(90.13,rf).pressed&Left); // The valid new edge was not discarded.
 for(int reason=0;reason<8;++reason){
  Consumer held;silent_scope::dc_xr::InputBridge h;h.set_sink(held.sink());auto q=rt;q.host_seconds=100;q.ui_select=false;
  auto g=rf;g.generation=1;g.poll_sequence=1;g.native_seconds=100;h.submit(q);held.poll(100,g);
  q.host_seconds=100.01;q.ui_select=true;h.submit(q);g.poll_sequence=2;g.native_seconds=100.01;REQUIRE(held.poll(100.01,g).pressed&Left);
  q.host_seconds=100.02;
  if(reason==0)q.focused=false;
  if(reason==1)q.tracked=false;
  if(reason==2)q.menu=true;
  if(reason==3)q.hand=0;
  if(reason==4)q.ui_main_menu=false;
  if(reason==5)h.invalidate();
  h.submit(q);g.poll_sequence=3;g.native_seconds=100.02;
  if(reason==6)g.generation=2;
  if(reason==7)g.main_menu_verified=false;
  REQUIRE(!held.poll(100.02,g).pressed); // No held Left crosses any phase/safety barrier.
 }
 for(int reason=0;reason<2;++reason){
  Consumer dup;silent_scope::dc_xr::InputBridge d;d.set_sink(dup.sink());auto q=rt;q.host_seconds=110;q.ui_select=false;
  auto g=rf;g.generation=1;g.poll_sequence=1;g.native_seconds=110;d.submit(q);dup.poll(110,g);
  q.host_seconds=110.01;q.ui_select=true;d.submit(q);g.poll_sequence=2;g.native_seconds=110.01;REQUIRE(dup.poll(110.01,g).pressed&Left);
  q.host_seconds=reason==0?110.3:110.02;d.submit(q);if(reason==1)g.main_menu_verified=false;
  p=dup.poll(q.host_seconds,g);REQUIRE(p.status==Status::DuplicatePoll&&!p.pressed&&!p.wheel);
 }
#endif
 return 0;
}
