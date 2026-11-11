#include <iostream>
#if __has_include("menu_controller.h")
#include "menu_controller.h"
#include "cabinet_controls.h"
#define HAS_MENU 1
#endif
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
#ifdef HAS_MENU
using namespace silent_scope;
struct Rig {
    MenuController menu;Settings settings;MenuFrame f;MenuCapabilities caps{true,true,true};
    Rig(){f.hands[0].tracked=f.hands[1].tracked=true;step();}
    MenuEffects step(double dt=.01){f.time+=dt;return menu.update(f,settings,caps);}
    MenuEffects click(MenuRow row,int hand=1,double u=.8){f.hands[hand].pointer_row=int(row);f.hands[hand].pointer_u=u;f.hands[hand].trigger=false;step();f.hands[hand].trigger=true;auto e=step();f.hands[hand].trigger=false;step();return e;}
    MenuEffects toggle(){f.menu_action=true;auto e=step();f.menu_action=false;step();return e;}
};
#endif
int main(){
#ifndef HAS_MENU
    REQUIRE(false && "pure menu policy has not been implemented");
#else
    Rig r;REQUIRE(!r.menu.open());REQUIRE(r.step().gameplay_allowed);
    r.f.hands[1].back=true;r.step();REQUIRE(!r.step(.79).release_inputs && !r.menu.open());
    auto e=r.step(.02);REQUIRE(r.menu.open() && e.release_inputs && !e.gameplay_allowed);
    r.f.hands[1].back=false;r.step();REQUIRE(r.menu.open());
    r.f.hands[1].back=true;r.step();r.f.hands[1].back=false;r.step(.1);REQUIRE(!r.menu.open());
    r.toggle();REQUIRE(r.menu.open());
    e=r.click(MenuRow::Hand,0);REQUIRE(e.save && e.release_inputs && r.settings.active_hand==0);
    REQUIRE(!r.click(MenuRow::Gun).save && !r.click(MenuRow::TwoPlayers).save && !r.click(MenuRow::Reload).save);
    e=r.click(MenuRow::Laser);REQUIRE(e.save && !r.settings.laser_on);
    r.settings.reticle=Reticle::None;const auto minimum=r.settings.reticle_min_size;
    REQUIRE(!r.click(MenuRow::ReticleMinSize).save && r.settings.reticle_min_size==minimum);
    r.click(MenuRow::Reticle);r.settings.world_size=1;r.click(MenuRow::WorldSize);REQUIRE(r.settings.world_size==1);
    r.click(MenuRow::WorldSize,1,.2);REQUIRE(std::abs(r.settings.world_size-.995)<1e-12);
    r.caps.wider_view_applied=false;REQUIRE(!r.click(MenuRow::HudSize).save);r.caps.wider_view_applied=true;
    r.click(MenuRow::HudSize);REQUIRE(r.settings.hud_size==1.05);
    e=r.click(MenuRow::Recenter);REQUIRE(e.recenter && e.release_inputs && !e.quit);
    r.click(MenuRow::HowToPlay);REQUIRE(r.menu.how_to_play());
    r.click(MenuRow::HowToPlay);REQUIRE(!r.menu.how_to_play());
    r.settings.calibration.pitch_center=-9;e=r.click(MenuRow::ResetDefaults);
    REQUIRE(e.save && e.release_inputs && r.menu.open() && !r.menu.how_to_play() && !e.service && !e.quit && r.settings.calibration.pitch_center==-9);
    e=r.click(MenuRow::Quit);REQUIRE(!e.quit && r.menu.quit_confirmation());
    r.click(MenuRow::Laser);REQUIRE(!r.menu.quit_confirmation());
    REQUIRE(!r.click(MenuRow::Quit).quit);e=r.click(MenuRow::Quit);REQUIRE(e.quit && e.release_inputs);
    Rig service;service.toggle();e=service.click(MenuRow::Service);REQUIRE(e.service && e.release_inputs && !service.menu.open() && !e.quit);
    Rig leaks;leaks.toggle();leaks.f.hands[1].select=true;leaks.toggle();REQUIRE(!leaks.menu.open() && !leaks.step().gameplay_allowed);
    leaks.f.hands[1].select=false;REQUIRE(!leaks.step().gameplay_allowed);REQUIRE(leaks.step().gameplay_allowed);
    Rig focus;focus.f.hands[0].back=true;focus.step();focus.f.focused=false;focus.step(.9);focus.f.focused=true;focus.step(.9);REQUIRE(!focus.menu.open());
    focus.f.hands[0].back=false;focus.step();focus.f.hands[0].back=true;focus.step();focus.step(.81);REQUIRE(focus.menu.open());
    Rig tracking;tracking.f.hands[0].back=true;tracking.step();tracking.f.hands[0].tracked=false;tracking.step(.9);tracking.f.hands[0].tracked=true;tracking.step(.9);REQUIRE(!tracking.menu.open());
    Rig clock;clock.f.hands[1].back=true;clock.step();clock.f.time=-10;clock.step(0);clock.step(20);REQUIRE(!clock.menu.open());
    clock.f.hands[1].back=false;clock.step();clock.f.hands[1].back=true;clock.step();clock.f.time=std::numeric_limits<double>::quiet_NaN();REQUIRE(!clock.step(0).gameplay_allowed && !clock.menu.open());
    Rig hand;hand.f.hands[1].back=true;hand.step();hand.settings.active_hand=0;hand.step(.9);REQUIRE(!hand.menu.open());
    Rig nav;nav.toggle();nav.f.hands[0].stick_y=-1;nav.step();REQUIRE(nav.menu.selected_row()==MenuRow::Recenter);nav.f.hands[0].stick_y=0;nav.step();nav.f.hands[0].select=true;e=nav.step();REQUIRE(e.recenter);
    auto items=menu_items(nav.settings,nav.caps);REQUIRE(items[int(MenuRow::Gun)].value=="Rifle" && !items[int(MenuRow::TwoPlayers)].enabled);
    REQUIRE(items[int(MenuRow::Reload)].value=="Automatic" && items[int(MenuRow::Reticle)].value=="Custom");
    Rig simultaneous;simultaneous.toggle();
    for(auto& input:simultaneous.f.hands){input.pointer_row=int(MenuRow::Quit);input.trigger=true;}
    e=simultaneous.step();REQUIRE(!e.quit && simultaneous.menu.quit_confirmation());
    Rig interruption;interruption.toggle();interruption.click(MenuRow::Quit);
    interruption.f.focused=false;interruption.step();interruption.f.focused=true;interruption.step();
    REQUIRE(!interruption.menu.quit_confirmation() && !interruption.click(MenuRow::Quit).quit);
    Rig recentered;recentered.f.hands[1].back=true;recentered.step();recentered.f.recentered=true;recentered.step(.4);
    recentered.f.recentered=false;recentered.step(.5);REQUIRE(!recentered.menu.open());
    Rig invalid_pointer;invalid_pointer.toggle();invalid_pointer.f.hands[0].pointer_row=int(MenuRow::Laser);
    invalid_pointer.f.hands[0].pointer_u=std::numeric_limits<double>::quiet_NaN();invalid_pointer.f.hands[0].trigger=true;
    REQUIRE(!invalid_pointer.step().save && invalid_pointer.settings.laser_on);
    Rig sparse;sparse.f.hands[1].back=true;sparse.step();sparse.f.hands[1].back=false;sparse.step(.9);
    REQUIRE(sparse.menu.open());sparse.f.hands[1].back=true;sparse.step();sparse.f.hands[1].back=false;sparse.step(.9);REQUIRE(sparse.menu.open());
    Rig negative;negative.f.time=-2;negative.f.menu_action=true;e=negative.step(0);
    REQUIRE(e.release_inputs && !negative.menu.open() && !e.gameplay_allowed);
    Rig native_gate;CabinetControls cabinet;
    auto dispatch=[&](double dt){auto policy=native_gate.step(dt);if(policy.release_inputs)cabinet.suspend();return cabinet.update(native_gate.f.time,policy.gameplay_allowed,{false,native_gate.f.hands[1].select,false,false});};
    dispatch(.01);native_gate.f.hands[1].select=true;REQUIRE(dispatch(.01).coin);
    native_gate.f.menu_action=true;auto output=dispatch(.01);REQUIRE(!output.coin && !output.start);
    native_gate.f.menu_action=false;dispatch(.01);native_gate.f.hands[1].select=false;dispatch(.01);
    native_gate.f.menu_action=true;dispatch(.01);native_gate.f.menu_action=false;dispatch(.01);
    for(int i=0;i<100;++i){output=dispatch(.01);REQUIRE(!output.coin && !output.start);}
    Rig haptic;haptic.toggle();haptic.settings.haptics=Haptics::Strong;
    REQUIRE(!haptic.click(MenuRow::Haptics).save && haptic.settings.haptics==Haptics::Strong);
    haptic.settings.haptics=Haptics::Off;REQUIRE(!haptic.click(MenuRow::Haptics,0,.2).save && haptic.settings.haptics==Haptics::Off);
    REQUIRE(haptic.click(MenuRow::Haptics).save && haptic.settings.haptics==Haptics::Light);
    Rig offhand;offhand.f.hands[1].tracked=false;offhand.f.hands[1].select=true;
    offhand.toggle();REQUIRE(offhand.menu.open());REQUIRE(offhand.click(MenuRow::Laser,0).save && !offhand.settings.laser_on);
    offhand.toggle();REQUIRE(!offhand.menu.open() && !offhand.step().gameplay_allowed);
    offhand.f.hands[1].tracked=true;offhand.f.hands[1].select=false;REQUIRE(!offhand.step().gameplay_allowed);REQUIRE(offhand.step().gameplay_allowed);
    Rig stale;stale.f.hands[0].tracked=false;stale.f.hands[0].trigger=true;stale.step();REQUIRE(stale.step().gameplay_allowed);
    auto pending=menu_items(stale.settings,{true,false,true});REQUIRE(pending[int(MenuRow::WiderView)].value=="On (pending)");
    Rig loss;loss.f.hands[0].back=true;loss.step();loss.f.hands[1].tracked=false;loss.step(.9);REQUIRE(!loss.menu.open());
    loss.f.hands[0].back=false;loss.step();loss.f.hands[0].back=true;loss.step();loss.step(.81);REQUIRE(loss.menu.open());
    Rig help;help.toggle();help.click(MenuRow::HowToPlay);help.f.hands[0].stick_y=-1;help.step();help.f.hands[0].stick_y=0;help.step();
    REQUIRE(help.menu.selected_row()==MenuRow::HowToPlay && help.menu.how_to_play());
    help.f.hands[0].select=true;e=help.step();REQUIRE(!e.service && !e.save && !e.quit && !help.menu.how_to_play() && help.menu.open());
    help.f.hands[0].select=false;help.step();help.click(MenuRow::HowToPlay);
    const auto angle=help.settings.gun_angle;e=help.click(MenuRow::GunAngle);REQUIRE(!e.save && help.settings.gun_angle==angle && help.menu.how_to_play());
    e=help.click(MenuRow::Service);REQUIRE(!e.service && help.menu.how_to_play());
    e=help.click(MenuRow::Quit);REQUIRE(!e.quit && !help.menu.quit_confirmation() && help.menu.how_to_play());
    help.click(MenuRow::HowToPlay);REQUIRE(!help.menu.how_to_play());
    Rig menu_recenter;menu_recenter.toggle();menu_recenter.f.hands[0].back=true;menu_recenter.step();
    e=menu_recenter.click(MenuRow::Recenter);REQUIRE(e.recenter && e.release_inputs);
    menu_recenter.f.hands[0].back=false;menu_recenter.step(.1);REQUIRE(menu_recenter.menu.open());
    menu_recenter.f.hands[0].back=true;menu_recenter.step();menu_recenter.f.hands[0].back=false;menu_recenter.step(.1);REQUIRE(!menu_recenter.menu.open());
    Rig same_frame;same_frame.toggle();same_frame.f.hands[1].back=true;same_frame.step();
    same_frame.f.hands[1].back=false;same_frame.f.hands[0].pointer_row=int(MenuRow::Recenter);same_frame.f.hands[0].trigger=true;
    e=same_frame.step(.1);REQUIRE(e.recenter && same_frame.menu.open());
#endif
    std::cout<<"Menu holds, selection, interruption, hand ownership and safe actions passed\n";
}
