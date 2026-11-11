// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_settings.h"
#include "dc_scope_camera.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "emulator.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace silent_scope { namespace dc_settings {
bool is_silent_scope() noexcept{return settings.content.gameId=="T9507N";}
namespace {
double number(const char* key,double fallback,double minimum,double maximum,bool integral=false) {
    std::istringstream input(config::loadStr("config",key,""));input.imbue(std::locale::classic());double value{};
    if(!(input>>value)||!std::isfinite(value)||value<minimum||value>maximum||(integral&&std::floor(value)!=value))return fallback;
    input>>std::ws;return input.eof()?value:fallback;
}
std::string word(const char* key,const char* fallback) {
    std::istringstream input(config::loadStr("config",key,""));std::string value;if(!(input>>value))return fallback;
    input>>std::ws;if(!input.eof())return fallback;
    for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    return value;
}
bool boolean(const char* key,bool fallback) {
    const auto value=word(key,"");if(value=="yes"||value=="true"||value=="1")return true;
    if(value=="no"||value=="false"||value=="0")return false;
    return fallback;
}
class PrimarySetting final:public config::Option<int,false> {
    using Base=config::Option<int,false>;
public:PrimarySetting():Base("vr.PrimaryHand",1){}
    void load()override{Base::load();set(int(number("vr.PrimaryHand",1,0,1,true)));}
    void save()const override{if(is_silent_scope())Base::save();}
} primary;
class FpsSetting final:public config::Option<bool,false> {
    using Base=config::Option<bool,false>;
public:FpsSetting():Base("vr.FpsCounter",false){}
    void load()override{Base::load();set(boolean("vr.FpsCounter",false));}
    void save()const override{if(is_silent_scope())Base::save();}
} fps;
class SmoothingSetting final:public config::Option<int,false> {
    using Base=config::Option<int,false>;
public:SmoothingSetting():Base("vr.RifleSmoothing",1){}
    void load()override{Base::load();set(int(number("vr.RifleSmoothing",1,0,3,true)));}
    void save()const override{if(is_silent_scope())Base::save();}
} smoothing;
class ScopeStabilitySetting final:public config::Option<int,false> {
    using Base=config::Option<int,false>;
public:ScopeStabilitySetting():Base("vr.ScopeStabilityLevel",1){}
    void load()override{
        Base::load();
        set(int(number("vr.ScopeStabilityLevel",boolean("vr.ScopeStability",true)?1:0,0,5,true)));
    }
    void save()const override{if(is_silent_scope())Base::save();}
} stability;
int scopeZoom=0;
class ScopeFitSetting final:public config::Option<int,false> {
    using Base=config::Option<int,false>;
    const char* key_;
    int default_,minimum_,maximum_;
public:
    ScopeFitSetting(const char* key,int initial,int minimum,int maximum)
        :Base(key,initial),key_(key),default_(initial),minimum_(minimum),maximum_(maximum){}
    void load()override{Base::load();set(int(number(key_,default_,minimum_,maximum_,true)));}
    void save()const override{if(is_silent_scope())Base::save();}
};
constexpr char ScopeDepthKey[]="vr.ScopeImageDepthMm", ScopeSizeKey[]="vr.ScopeImageSizePercent";
constexpr int DefaultScopeDepth=-27, DefaultScopeSize=125;
ScopeFitSetting scopeDepth(ScopeDepthKey,DefaultScopeDepth,-30,30), scopeSize(ScopeSizeKey,DefaultScopeSize,50,150);
constexpr char RifleModeKey[]="vr.RifleMode";
ScopeFitSetting rifleMode(RifleModeKey,0,0,1);
constexpr char AwpDepthKey[]="vr.AwpScopeImageDepthMm", AwpSizeKey[]="vr.AwpScopeImageSizePercent";
constexpr int DefaultAwpDepth=-8, DefaultAwpSize=110;
ScopeFitSetting awpDepth(AwpDepthKey,DefaultAwpDepth,-30,30), awpSize(AwpSizeKey,DefaultAwpSize,50,150);
ScopeFitSetting cabinetGunSize("vr.CabinetSizePercent",100,40,160), awpGunSize("vr.AwpSizePercent",100,40,160);
ScopeFitSetting laserWidth("vr.LaserWidth",1,1,4), widenMode("vr.WidenMode",1,0,2);
class PanelSetting final:public config::Option<float,false> {
    using Base=config::Option<float,false>;
public:PanelSetting():Base("vr.PanelDistanceMeters",1.8f){}
    void load()override{Base::load();set(float(number("vr.PanelDistanceMeters",1.8,.5,10)));}
    void save()const override{if(is_silent_scope())Base::save();}
} panel;
RuntimeCapabilities capabilities;uint64_t revision{};
const char* const haptic_words[]{"off","light","medium","strong"};
const char* const haptic_labels[]{"Off","Light","Medium","Strong"};
int haptic_level(const std::string& text) {
    for(int i=0;i<4;++i)if(text==haptic_words[i]||text==std::to_string(i))return i;
    if(text=="low")return 1;
    return 2;
}
void apply_panel() {
    if(!is_silent_scope()||!std::isfinite(capabilities.game_units_per_metre)||capabilities.game_units_per_metre<=0)return;
    const double depth=double(panel.get())*capabilities.game_units_per_metre;
    if(std::isfinite(depth)&&depth>0&&depth<=std::numeric_limits<float>::max())config::VrHudDepth.set(float(depth));
}
template<class Option,class Value,class Save>bool assign(Option& option,Value value,const char* key,Save save) {
    if(option.get()==value)return false;
    option.set(value);save("config",key,value);++revision;return true;
}
template<class Option>bool step(Option& option,const char* key,int dir,float amount,float lo,float hi) {
    const float next=std::clamp(std::round((option.get()+amount*dir)*1000000.f)/1000000.f,lo,hi);
    return assign(option,next,key,config::saveFloat);
}
std::string formatted(double value,int precision) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(precision)<<value;return out.str();
}
}
void sanitize_loaded() {
    if(!is_silent_scope())return;
    primary.load();fps.load();panel.load();smoothing.load();
    cabinetGunSize.load();awpGunSize.load();
    stability.load();scopeDepth.load();scopeSize.load();rifleMode.load();awpDepth.load();awpSize.load();
    config::VrGunAngle.set(int(number("vr.GunAngle",0,-30,30,true)));
    config::VrLaser.set(boolean("vr.Laser",true));laserWidth.load();
    const int mode=int(number("vr.WidenMode",boolean("vr.Widen",true)?1:0,0,2,true));
    widenMode.set(mode);config::VrWiden.set(mode!=0);
    config::VrReticleMode.set(int(number("vr.ReticleMode",1,1,2,true)));
    config::VrReticleMinSize.set(float(number("vr.ReticleMinSize",.25,.05,1)));
    config::VrReticleMaxSize.set(float(number("vr.ReticleMaxSize",1,.25,3)));
    config::VrReticleShrinkDistance.set(float(number("vr.ReticleShrinkDistance",6,.2,20)));
    config::VrHaptics.set(haptic_words[haptic_level(word("vr.Haptics","medium"))]);
    config::VrWorldScale.set(float(number("vr.WorldScale",.1,.01,1)));
    config::VrHudSize.set(float(number("vr.HudSize",1,.5,2)));
    // Unsupported two-player/manual slide controls never become active merely
    // because an earlier generic profile left those options enabled.
    config::VrDualWield.set(false);config::VrSlideReload.set(false);config::VrGameHands.set(false);
    apply_panel();
}
void set_runtime_capabilities(RuntimeCapabilities value) {
    if(!is_silent_scope())return;
    value.wider_applied=value.wider_available&&value.wider_applied;
    if(!std::isfinite(value.game_units_per_metre)||value.game_units_per_metre<=0)value.game_units_per_metre=0;
    capabilities=value;apply_panel();
}
RuntimeCapabilities runtime_capabilities() noexcept{return capabilities;}
int primary_hand() noexcept{return primary.get()==0?0:1;}
bool fps_counter() noexcept{return fps.get();}
int aim_smoothing_level() noexcept{return smoothing.get()>=0&&smoothing.get()<=3?smoothing.get():1;}
bool simulation_mode() noexcept{return rifleMode.get()==1;}
float rifle_user_scale(bool simulation) noexcept{return (simulation?awpGunSize:cabinetGunSize).get()*.01f;}
float rifle_model_scale(bool simulation) noexcept{return rifle_user_scale(simulation)*(simulation?1.10f:1.f);}
float rifle_user_scale() noexcept{return rifle_user_scale(simulation_mode());}
float rifle_model_scale() noexcept{return rifle_model_scale(simulation_mode());}
float scope_image_offset() noexcept{return (simulation_mode()?awpDepth:scopeDepth).get()*.001f;}
float scope_image_scale() noexcept{return (simulation_mode()?awpSize:scopeSize).get()*.01f;}
int scope_stability_level() noexcept{return std::clamp(stability.get(),0,5);}
float scope_magnification() noexcept{
    constexpr float levels[]{.2f,.5f,1.f,2.f,5.f};
    return levels[scopeZoom+2];
}
void reset_scope_zoom() noexcept {
    if(scopeZoom!=0){scopeZoom=0;++revision;}
    dreamcast::scope_camera_magnification.store(1.f,std::memory_order_relaxed);
}
float panel_distance_metres() noexcept{return panel.get();}
int laser_choice() noexcept {
    return !config::VrLaser.get()?0:laserWidth.get()==3?2:laserWidth.get()==4?3:1;
}
bool select_laser_choice(int next) {
    if(!is_silent_scope()||next<0||next>3||next==laser_choice())return false;
    config::VrLaser.set(next!=0);
    if(next)laserWidth.set(next==2?3:next==3?4:1);
    config::setAutoSave(false);
    try{
        config::saveBool("config","vr.Laser",next!=0);
        config::saveInt("config","vr.LaserWidth",laserWidth.get());
    }catch(...){config::setAutoSave(true);throw;}
    config::setAutoSave(true);++revision;return true;
}
float laser_half_width() noexcept {
    const float multipliers[]{2.f,2.f,5.f,10.f};
    return .0008f*multipliers[std::clamp(laserWidth.get(),1,4)-1];
}
int wider_view_mode() noexcept{return widenMode.get();}
bool toggle_aiming_aids() {
    if(!is_silent_scope())return false;
    const bool enabled=!config::VrLaser.get()&&config::VrReticleMode.get()==2;
    config::setAutoSave(false);
    try{
        config::VrLaser.set(enabled);config::VrReticleMode.set(enabled?1:2);
        config::saveBool("config","vr.Laser",enabled);
        config::saveInt("config","vr.ReticleMode",enabled?1:2);
    }catch(...){config::setAutoSave(true);throw;}
    config::setAutoSave(true);++revision;
    return true;
}
uint64_t settings_revision() noexcept{return revision;}
bool set_primary_hand(int hand){return is_silent_scope()&&hand>=0&&hand<=1&&assign(primary,hand,"vr.PrimaryHand",config::saveInt);}
bool set_fps_counter(bool on){return is_silent_scope()&&assign(fps,on,"vr.FpsCounter",config::saveBool);}
bool enabled(Setting setting) noexcept {
    if(!is_silent_scope())return false;
    if(setting==Setting::PanelDistance)return capabilities.game_units_per_metre>0;
    if(setting==Setting::HudSize)return capabilities.wider_applied;
    if(setting==Setting::WiderView)return capabilities.wider_available;
    if(setting==Setting::ReticleMinimum||setting==Setting::ReticleMaximum||setting==Setting::ReticleDistance)return config::VrReticleMode.get()==1;
    return true;
}
bool change(Setting setting,int direction) {
    if(!enabled(setting))return false;
    const int dir=direction<0?-1:1;
    switch(setting) {
    case Setting::GunSize:{
        auto& size=simulation_mode()?awpGunSize:cabinetGunSize;
        return assign(size,std::clamp(size.get()+dir*5,40,160),
            simulation_mode()?"vr.AwpSizePercent":"vr.CabinetSizePercent",config::saveInt);
    }
    case Setting::GunAngle:return assign(config::VrGunAngle,std::clamp(config::VrGunAngle.get()+dir,-30,30),"vr.GunAngle",config::saveInt);
    case Setting::Hand:return set_primary_hand(1-primary_hand());
    case Setting::FpsCounter:return set_fps_counter(!fps_counter());
    case Setting::AimSmoothing:return assign(smoothing,std::clamp(aim_smoothing_level()+dir,0,3),"vr.RifleSmoothing",config::saveInt);
    case Setting::ScopeStability:return assign(stability,std::clamp(scope_stability_level()+dir,0,5),"vr.ScopeStabilityLevel",config::saveInt);
    case Setting::RifleMode:return assign(rifleMode,std::clamp(rifleMode.get()+dir,0,1),RifleModeKey,config::saveInt);
    case Setting::ScopeImageDepth:{
        auto& depth=simulation_mode()?awpDepth:scopeDepth;
        return assign(depth,std::clamp(depth.get()+dir,-30,30),simulation_mode()?AwpDepthKey:ScopeDepthKey,config::saveInt);
    }
    case Setting::ScopeImageSize:{
        auto& size=simulation_mode()?awpSize:scopeSize;
        return assign(size,std::clamp(size.get()+5*dir,50,150),simulation_mode()?AwpSizeKey:ScopeSizeKey,config::saveInt);
    }
    case Setting::ScopeZoom:{
        const int next=std::clamp(scopeZoom+dir,-2,2);
        if(next==scopeZoom)return false;
        scopeZoom=next;
        dreamcast::scope_camera_magnification.store(scope_magnification(),std::memory_order_relaxed);
        ++revision;
        return true;
    }
    case Setting::Laser:return select_laser_choice(std::clamp(laser_choice()+dir,0,3));
    case Setting::Reticle:return assign(config::VrReticleMode,config::VrReticleMode.get()==2?1:2,"vr.ReticleMode",config::saveInt);
    case Setting::ReticleMinimum:return step(config::VrReticleMinSize,"vr.ReticleMinSize",dir,.01f,.05f,1.f);
    case Setting::ReticleMaximum:return step(config::VrReticleMaxSize,"vr.ReticleMaxSize",dir,.05f,.25f,3.f);
    case Setting::ReticleDistance:return step(config::VrReticleShrinkDistance,"vr.ReticleShrinkDistance",dir,.1f,.2f,20.f);
    case Setting::Haptics:{const int level=std::clamp(haptic_level(config::VrHaptics.get())+dir,0,3);return assign(config::VrHaptics,std::string(haptic_words[level]),"vr.Haptics",config::saveStr);}
    case Setting::WorldSize:return step(config::VrWorldScale,"vr.WorldScale",dir,.005f,.01f,1.f);
    case Setting::PanelDistance:{const bool changed=step(panel,"vr.PanelDistanceMeters",dir,.1f,.5f,10.f);apply_panel();return changed;}
    case Setting::HudSize:return step(config::VrHudSize,"vr.HudSize",dir,.05f,.5f,2.f);
    case Setting::WiderView:{
        const int next=std::clamp(widenMode.get()+dir,0,2);
        if(next==widenMode.get())return false;
        widenMode.set(next);config::VrWiden.set(next!=0);
        config::setAutoSave(false);
        try{
            config::saveInt("config","vr.WidenMode",next);
            config::saveBool("config","vr.Widen",next!=0);
        }catch(...){config::setAutoSave(true);throw;}
        config::setAutoSave(true);++revision;return true;
    }
    }
    return false;
}
std::string value(Setting setting) {
    switch(setting) {
    case Setting::GunSize:return std::to_string((simulation_mode()?awpGunSize:cabinetGunSize).get())+"%";
    case Setting::GunAngle:return std::to_string(config::VrGunAngle.get());
    case Setting::Hand:return primary_hand()==0?"Left":"Right";
    case Setting::FpsCounter:return fps_counter()?"On":"Off";
    case Setting::AimSmoothing:{const char* const names[]{"Off","Low","Medium","High"};return names[aim_smoothing_level()];}
    case Setting::ScopeStability:return scope_stability_level()==0?"Off":std::to_string(scope_stability_level());
    case Setting::RifleMode:return simulation_mode()?"Simulation":"Arcade";
    case Setting::ScopeImageDepth:{
        const int depth=(simulation_mode()?awpDepth:scopeDepth).get();
        return std::to_string(std::abs(depth))+" mm"+(depth>0?" rear":depth<0?" forward":"");
    }
    case Setting::ScopeImageSize:return std::to_string((simulation_mode()?awpSize:scopeSize).get())+"%";
    case Setting::ScopeZoom:{const char* const names[]{"0.2x","0.5x","1x","2x","5x"};return names[std::clamp(scopeZoom,-2,2)+2];}
    case Setting::Laser:{const char* names[]{"Off","Normal","5x","10x"};return names[laser_choice()];}
    case Setting::Reticle:return config::VrReticleMode.get()==2?"None":"Custom";
    case Setting::ReticleMinimum:return formatted(config::VrReticleMinSize.get()*100,0)+"%";
    case Setting::ReticleMaximum:return formatted(config::VrReticleMaxSize.get()*100,0)+"%";
    case Setting::ReticleDistance:return formatted(config::VrReticleShrinkDistance.get(),1)+" m";
    case Setting::Haptics:return haptic_labels[haptic_level(config::VrHaptics.get())];
    case Setting::WorldSize:return formatted(config::VrWorldScale.get(),3);
    case Setting::PanelDistance:return formatted(panel.get(),1)+(enabled(setting)?" m":" m (pending)");
    case Setting::HudSize:return formatted(config::VrHudSize.get()*100,0)+"%";
    case Setting::WiderView:{
        if(!capabilities.wider_available)return "Unavailable";
        const char* names[]{"Off","On","Fade"};
        return std::string(names[widenMode.get()])+(widenMode.get()&&!capabilities.wider_applied?" (pending)":"");
    }
    }
    return {};
}
void reset_defaults() {
    if(!is_silent_scope())return;
    primary.set(1);config::saveInt("config","vr.PrimaryHand",1);fps.set(false);config::saveBool("config","vr.FpsCounter",false);
    panel.set(1.8f);config::saveFloat("config","vr.PanelDistanceMeters",1.8f);
    smoothing.set(1);config::saveInt("config","vr.RifleSmoothing",1);
    stability.set(1);config::saveInt("config","vr.ScopeStabilityLevel",1);
    awpDepth.set(DefaultAwpDepth);config::saveInt("config",AwpDepthKey,DefaultAwpDepth);
    awpSize.set(DefaultAwpSize);config::saveInt("config",AwpSizeKey,DefaultAwpSize);
    rifleMode.set(0);config::saveInt("config",RifleModeKey,0);
    scopeDepth.set(DefaultScopeDepth);config::saveInt("config",ScopeDepthKey,DefaultScopeDepth);
    scopeSize.set(DefaultScopeSize);config::saveInt("config",ScopeSizeKey,DefaultScopeSize);
    cabinetGunSize.set(100);config::saveInt("config","vr.CabinetSizePercent",100);
    awpGunSize.set(100);config::saveInt("config","vr.AwpSizePercent",100);
    reset_scope_zoom();
    config::VrGunAngle.set(0);config::saveInt("config","vr.GunAngle",0);
    config::VrLaser.set(true);config::saveBool("config","vr.Laser",true);
    laserWidth.set(1);config::saveInt("config","vr.LaserWidth",1);
    widenMode.set(1);config::saveInt("config","vr.WidenMode",1);
    config::VrReticleMode.set(1);config::saveInt("config","vr.ReticleMode",1);
    config::VrReticleMinSize.set(.25f);config::saveFloat("config","vr.ReticleMinSize",.25f);
    config::VrReticleMaxSize.set(1);config::saveFloat("config","vr.ReticleMaxSize",1);
    config::VrReticleShrinkDistance.set(6);config::saveFloat("config","vr.ReticleShrinkDistance",6);
    config::VrHaptics.set("medium");config::saveStr("config","vr.Haptics","medium");
    config::VrWorldScale.set(.1f);config::saveFloat("config","vr.WorldScale",.1f);
    config::VrHudSize.set(1);config::saveFloat("config","vr.HudSize",1);
    config::VrWiden.set(true);config::saveBool("config","vr.Widen",true);
    config::VrDualWield.set(false);config::VrSlideReload.set(false);config::VrGameHands.set(false);
    apply_panel();++revision;
}
}}
