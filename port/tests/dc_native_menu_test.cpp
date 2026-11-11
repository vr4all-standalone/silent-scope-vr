#include "xr_menu.h"
#include "xr_host.h"
#include "xr_panel.h"
#include "emulator.h"
#include "dreamcast/dc_settings.h"
#include "dreamcast/dc_persistence.h"
#include <iostream>
#define REQUIRE(x)do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;}}while(false)
namespace menu=vr::xr::menu;
namespace dc=silent_scope::dc_settings;
int main(){
 settings.content.gameId="T9507N";config::values.clear();dc::set_runtime_capabilities({true,false,10});
 menu::open(glm::mat4(1));menu::update({});menu::draw(glm::mat4(1));
 auto& panel=vr::xr::drawnSettings;
 REQUIRE(panel.game=="Silent Scope"&&panel.title=="Gun"&&panel.rows.size()==5);
 REQUIRE(panel.rows[0].label=="Gun angle"&&menu::pointerOwner()==1);
 for(const auto& row:panel.rows)REQUIRE(row.label!="Gun model");
 auto click=[](int hand,int row,float u=9.f){
  menu::Input in{};in.ray[hand]={true,true,{u,float(row),0},{0,0,-1}};
  menu::update(in);menu::draw(glm::mat4(1));menu::update({});
 };
 click(0,0,1);REQUIRE(!dc::simulation_mode()&&menu::pointerOwner()==0);
 dc::change(dc::Setting::RifleMode,1); // Physical pickup owns the selection now.
 click(0,4);REQUIRE(dc::simulation_mode()&&panel.title=="Advanced gun settings"&&panel.back);
 REQUIRE(panel.rows.size()==7&&panel.rows[0].label=="Gun size"&&panel.rows[1].label=="Aim smoothing"&&panel.rows[2].label=="Scope stability"&&panel.rows[3].label=="Scope zoom");
 for(const auto& row:panel.rows)REQUIRE(row.label!="Scope image depth"&&row.label!="Scope image size");
 click(0,1);REQUIRE(dc::aim_smoothing_level()==2);
 menu::Input stick{};stick.right=true;menu::update(stick);REQUIRE(dc::aim_smoothing_level()==3);
 menu::Input inactive{};inactive.perHand=true;inactive.hand[1].left=true;
 menu::update(inactive);REQUIRE(dc::aim_smoothing_level()==3);
 inactive.hand[1]={};inactive.hand[0].left=true;
 menu::update(inactive);REQUIRE(dc::aim_smoothing_level()==2);
 inactive.hand[0]={};
 inactive.ray[1]={true,true,{9.f,1,0},{0,0,-1}};
 inactive.hand[1].right=true;
 menu::update(inactive);REQUIRE(menu::pointerOwner()==1&&dc::aim_smoothing_level()==2);
 menu::update({});
 click(0,1);REQUIRE(menu::pointerOwner()==0&&dc::aim_smoothing_level()==2);
 menu::update({});

 click(0,3,-1);REQUIRE(dc::scope_magnification()==.5f);
 click(0,-2);REQUIRE(panel.title=="Gun");
 click(0,-1,1);REQUIRE(panel.title=="World");
 click(0,1);REQUIRE(panel.title=="Advanced world settings"&&panel.rows[1].label=="HUD size");
 click(0,-1,2);REQUIRE(panel.title=="System");
 click(0,0,0);REQUIRE(dc::primary_hand()==0);
 click(0,2,1);REQUIRE(dc::fps_counter());
 click(0,3);REQUIRE(panel.dialog&&dc::fps_counter());
 click(0,0);REQUIRE(!panel.dialog&&dc::fps_counter());
 config::VrGunFile.set("stale custom gun");config::flushes=0;
 click(0,3);click(0,1);
 REQUIRE(config::VrGunFile.get().empty()&&config::flushes==1&&config::auto_saving&&menu::isOpen()&&!dc::fps_counter()&&dc::aim_smoothing_level()==1);
 silent_scope::dc_persistence::error="cannot replace settings";menu::draw(glm::mat4(1));
 REQUIRE(panel.footer.find("Settings not saved: cannot replace settings")==0);
 silent_scope::dc_persistence::error.clear();
 click(0,4);REQUIRE(panel.dialog&&vr::xr::quit_calls==0);
 click(0,1);REQUIRE(vr::xr::quit_calls==1);
 menu::close();settings.content.gameId="OTHER";menu::open(glm::mat4(1));menu::update({});menu::draw(glm::mat4(1));
 REQUIRE(vr::xr::drawn.rows.size()==21&&vr::xr::drawn.rows[4].label=="Reload"&&vr::xr::drawn.rows[16].label=="Game zoom");
 std::cout<<"PASS patched Flycast card pages, controller ownership and other-title menu\n";
}
