// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "settings.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace silent_scope {
enum class MenuRow {
    Resume, Recenter, Gun, GunAngle, Hand, Reload, TwoPlayers, Laser, Reticle,
    ReticleMinSize, ReticleMaxSize, ReticleShrinkDistance, Haptics, WorldSize,
    PanelDistance, HudSize, WiderView, HowToPlay, Service, FpsCounter, ResetDefaults, Quit, Count
};
constexpr int menu_row_count=int(MenuRow::Count);
struct MenuCapabilities {
    bool wider_view_available{},wider_view_applied{},service_available{true};
    bool service_active{};
};
struct MenuItem {MenuRow row;const char* label;std::string value;bool enabled;};
inline std::string menu_number(double value,int precision=0) {
    std::ostringstream out;out<<std::fixed<<std::setprecision(precision)<<value;return out.str();
}
inline std::array<MenuItem,menu_row_count> menu_items(const Settings& s,MenuCapabilities cap={}) {
    const bool sizes=s.reticle==Reticle::Custom;
    const char* haptics[]{"Off","Light","Medium","Strong"};
    return {{{MenuRow::Resume,"Resume","",true},
        {MenuRow::Recenter,"Recenter","",true},
        {MenuRow::Gun,"Gun","Rifle",false},
        {MenuRow::GunAngle,"Gun angle",menu_number(s.gun_angle)+" deg",true},
        {MenuRow::Hand,"Hand",s.active_hand==0?"Left":"Right",true},
        {MenuRow::Reload,"Reload","Automatic",false},
        {MenuRow::TwoPlayers,"Two players","Unavailable",false},
        {MenuRow::Laser,"Laser",s.laser_on?"On":"Off",true},
        {MenuRow::Reticle,"Reticle",s.reticle==Reticle::Custom?"Custom":"None",true},
        {MenuRow::ReticleMinSize,"Reticle min size",menu_number(s.reticle_min_size*100)+"%",sizes},
        {MenuRow::ReticleMaxSize,"Reticle max size",menu_number(s.reticle_max_size*100)+"%",sizes},
        {MenuRow::ReticleShrinkDistance,"Reticle shrink distance",menu_number(s.reticle_shrink_distance,1)+" m",sizes},
        {MenuRow::Haptics,"Haptics",haptics[std::clamp(int(s.haptics),0,3)],true},
        {MenuRow::WorldSize,"World size",menu_number(s.world_size,3),true},
        {MenuRow::PanelDistance,"Panel distance",menu_number(s.panel_distance,1)+" m",true},
        {MenuRow::HudSize,"HUD size",cap.wider_view_applied?menu_number(s.hud_size*100)+"%":"Unavailable in Original",cap.wider_view_applied},
        {MenuRow::WiderView,"Wider view",cap.wider_view_available?(s.wider_view?(cap.wider_view_applied?"On":"On (pending)"):"Off"):"Unavailable",cap.wider_view_available},
        {MenuRow::HowToPlay,"How to play","",true},
        {MenuRow::Service,"Open Service Menu",cap.service_active?"Already open":cap.service_available?"":"Unavailable",cap.service_available&&!cap.service_active},
        {MenuRow::FpsCounter,"FPS counter",s.fps_counter?"On":"Off",true},
        {MenuRow::ResetDefaults,"Reset to defaults","",true},
        {MenuRow::Quit,"Quit","",true}}};
}
// Returns true only when a preference changes. Action rows are controller-owned.
inline bool adjust_menu_setting(Settings& s,MenuRow row,int direction,MenuCapabilities cap={}) {
    const int index=int(row);if(index<0||index>=menu_row_count||!menu_items(s,cap)[index].enabled)return false;
    direction=direction<0?-1:1;
    auto adjust=[&](double& value,double step,double minimum,double maximum){const auto previous=value;value=std::clamp(std::round((value+step*direction)*1000000)/1000000,minimum,maximum);return value!=previous;};
    switch(row) {
    case MenuRow::GunAngle:return adjust(s.gun_angle,1,-30,30);
    case MenuRow::Hand:s.active_hand=1-s.active_hand;return true;
    case MenuRow::Laser:s.laser_on=!s.laser_on;return true;
    case MenuRow::Reticle:s.reticle=s.reticle==Reticle::Custom?Reticle::None:Reticle::Custom;return true;
    case MenuRow::ReticleMinSize:return adjust(s.reticle_min_size,.01,.05,1);
    case MenuRow::ReticleMaxSize:return adjust(s.reticle_max_size,.05,.25,3);
    case MenuRow::ReticleShrinkDistance:return adjust(s.reticle_shrink_distance,.1,.2,20);
    case MenuRow::Haptics:{const auto previous=s.haptics;s.haptics=Haptics(std::clamp(int(s.haptics)+direction,0,3));return s.haptics!=previous;}
    case MenuRow::WorldSize:return adjust(s.world_size,.005,.01,1);
    case MenuRow::PanelDistance:return adjust(s.panel_distance,.1,.5,10);
    case MenuRow::HudSize:return adjust(s.hud_size,.05,.5,2);
    case MenuRow::WiderView:s.wider_view=!s.wider_view;return true;
    case MenuRow::FpsCounter:s.fps_counter=!s.fps_counter;return true;
    default:return false;
    }
}
}
