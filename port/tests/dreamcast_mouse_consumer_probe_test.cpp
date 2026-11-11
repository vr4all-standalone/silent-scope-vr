#include <cmath>
#include <iostream>
#include <vector>
#if __has_include("dreamcast/dc_mouse_consumer_probe.h")
#include "dreamcast/dc_mouse_consumer_probe.h"
#define HAS_PROBE 1
#endif
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
#ifndef HAS_PROBE
 REQUIRE(false&&"Consumer probe missing");
#else
 using namespace silent_scope::dc_mouse_consumer_probe;
 REQUIRE(!recipe(39.999,100).owns);
 REQUIRE(!recipe(std::nan(""),100).owns);
 REQUIRE(!recipe(42,std::nan("")).owns);
 const double times[]{40,42,44,46,48,50,52,54,56,60,65};
 const double yaw[]{0,-40,0,-120,0,0,0,0,0,0,0};
 const double pitch[]{0,0,0,0,0,-40,0,-120,0,0,0};
 silent_scope::dc_xr::InputBridge bridge;silent_scope::dc_mouse::Consumer consumer;
 bridge.set_sink(consumer.sink());
 for(unsigned i=0;i<11;++i){
  const auto r=recipe(times[i],100+i);REQUIRE(r.owns&&r.target.direction_valid&&r.target.focused&&r.target.tracked);
  REQUIRE(!r.target.fire&&!r.target.start&&!r.target.ui_select&&!r.target.point_valid);
  silent_scope::dc_xr::Target emitted;REQUIRE(bridge.submit(r.target,&emitted));
  REQUIRE(std::abs(emitted.yaw_units-yaw[i])<1e-9&&std::abs(emitted.pitch_units-pitch[i])<1e-9);
 }
 Runner runner;std::uint8_t buttons=0;int x=50,y=50,wheel=50;
 REQUIRE(!runner.poll("OTHER",40,100,nullptr,0,1,buttons,x,y,wheel));REQUIRE(x==50&&buttons==0);
 REQUIRE(runner.poll("T9507N",40,100,nullptr,0,1,buttons,x,y,wheel));
 REQUIRE(x==0&&y==0&&wheel==0&&(buttons&silent_scope::dc_mouse::OwnedButtons)==silent_scope::dc_mouse::OwnedButtons);
 REQUIRE(runner.last().packet.status==silent_scope::dc_mouse::Status::InvalidContext);
 REQUIRE(runner.poll("T9507N",60,101,nullptr,0,1,buttons,x,y,wheel));REQUIRE(!x&&!y&&!wheel);
 // Synthetic RAM validates composition of the real reader/bridge/consumer;
 // this fixture does not emulate the game or claim native runtime closure.
 using namespace silent_scope::dc_mouse;
 std::vector<std::uint8_t> ram(16u*1024*1024);
 const auto w16=[&](std::uint32_t a,std::uint16_t v){a&=0xffffff;ram[a]=v&255;ram[a+1]=v>>8;};
 const auto w32=[&](std::uint32_t a,std::uint32_t v){w16(a,std::uint16_t(v));w16(a+2,std::uint16_t(v>>16));};
 for(auto g:MouseWordGuards)w16(g.first,g.second);
 for(auto g:MouseLongGuards)w32(g.first,g.second);
 for(auto g:MouseTableGuards)w32(g.first,g.second);
 for(unsigned i=0;i<MouseClampGuards.size();++i)w16(0x8c073e7eu + 2*i,MouseClampGuards[i]);
 w16(0x8c073ee4,5500);w16(0x8c073ee6,std::uint16_t(-5500));w16(0x8c073ee8,4300);w16(0x8c073eea,std::uint16_t(-4300));
 w32(0x8c1d54ac,0x8c300000);w32(0x8c26fe1c,1);w32(0x8c1b013c,1);ram[0x1b015c]=255;
 Runner loop;
 REQUIRE(loop.poll("T9507N",40,200,ram.data(),ram.size(),2,buttons,x,y,wheel));REQUIRE(!x&&!y);
 REQUIRE(loop.poll("T9507N",42,202,ram.data(),ram.size(),2,buttons,x,y,wheel));REQUIRE(x==16&&!y);
 REQUIRE(loop.last().feedback.code_verified&&loop.last().feedback.gain.verified&&loop.last().emitted.yaw_units<0);
 w16(0x8c1b0862,std::uint16_t(-20));loop.poll("T9507N",42.01,202.01,ram.data(),ram.size(),2,buttons,x,y,wheel);REQUIRE(!x);
 w16(0x8c1b0862,std::uint16_t(-40));loop.poll("T9507N",42.02,202.02,ram.data(),ram.size(),2,buttons,x,y,wheel);REQUIRE(!x);
 loop.poll("T9507N",42.03,202.03,ram.data(),ram.size(),2,buttons,x,y,wheel);REQUIRE(loop.last().packet.aligned&&loop.last().packet.learned_yaw&&!x);
 loop.poll("T9507N",44,204,ram.data(),ram.size(),2,buttons,x,y,wheel);REQUIRE(x==-16&&!y);
 loop.poll("T9507N",60,220,ram.data(),ram.size(),2,buttons,x,y,wheel);REQUIRE(!x&&!y);
 REQUIRE((buttons&OwnedButtons)==OwnedButtons);
 REQUIRE(recipe(46.4,300).target.fire==false); // Existing default stays no-fire.
 for(double start:{42.4,44.4,46.4}){
  REQUIRE(!recipe(start-.001,300,true).target.fire);
  REQUIRE(recipe(start,300,true).target.fire);
  REQUIRE(recipe(start+.099,300,true).target.fire);
  REQUIRE(!recipe(start+.1,300,true).target.fire);
 }
 REQUIRE(!recipe(60,300,true).target.fire);
 w16(0x8c1b0862,std::uint16_t(-120));w16(0x8c1b0860,0);
 Runner fire;
 fire.poll("T9507N",46.39,300,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 fire.poll("T9507N",46.4,300.01,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 REQUIRE((buttons&Left)==0&&fire.last().packet.fresh_fire&&fire.last().delivered_buttons==Left);
 fire.poll("T9507N",46.45,300.06,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 REQUIRE((buttons&Left)==0&&!fire.last().packet.fresh_fire);
 fire.poll("T9507N",46.46,300.07,ram.data(),ram.size(),2,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 fire.poll("T9507N",46.47,300.08,ram.data(),ram.size(),2,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 fire.poll("T9507N",46.5,300.11,ram.data(),ram.size(),2,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 // Native fire delivery is independent of aim convergence; the motion ACK
 // and native button are still separate observable facts.
 w16(0x8c1b0862,0);Runner queued;
 queued.poll("T9507N",40,400,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 queued.poll("T9507N",46.39,406.39,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(x==16);
 w16(0x8c1b0862,std::uint16_t(-40));
 queued.poll("T9507N",46.4,406.4,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 REQUIRE((buttons&Left)==0&&queued.last().packet.fresh_fire);
 queued.poll("T9507N",46.41,406.41,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(x==32&&(buttons&Left)==0);
 w16(0x8c1b0862,std::uint16_t(-120));
 queued.poll("T9507N",46.43,406.43,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 REQUIRE((buttons&Left)==0&&!queued.last().packet.fresh_fire);
 queued.poll("T9507N",46.5,406.5,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 queued.poll("T9507N",60,420,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255&&!x&&!y);
 Runner pulses;w16(0x8c1b0862,0);w16(0x8c1b0860,0);
 pulses.poll("T9507N",40,500,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 for(double start:{42.4,44.4,46.4}){
  w16(0x8c1b0862,std::uint16_t(start<44?-40:start<46?0:-120));w16(0x8c1b0860,0);
  pulses.poll("T9507N",start-.01,500+start,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
  pulses.poll("T9507N",start,500+start+.01,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
  REQUIRE(pulses.last().packet.fresh_fire&&(buttons&Left)==0&&(buttons&(Right|Middle))==(Right|Middle));
  pulses.poll("T9507N",start+.1,500+start+.11,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 }
 Runner invalid;w16(0x8c1b0862,std::uint16_t(-120));w16(0x8c1b0860,0);
 invalid.poll("T9507N",46.39,600,ram.data(),ram.size(),1,buttons,x,y,wheel,true);
 w16(MouseWordGuards[0].first,0);
 invalid.poll("T9507N",46.4,600.01,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
 w16(MouseWordGuards[0].first,MouseWordGuards[0].second);
 invalid.poll("T9507N",46.41,600.02,ram.data(),ram.size(),1,buttons,x,y,wheel,true);REQUIRE(buttons==255);
#endif
 return 0;
}
