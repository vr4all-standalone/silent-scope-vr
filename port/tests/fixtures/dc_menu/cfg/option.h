#pragma once
#include "cfg.h"
#include <vector>
namespace config {
struct BaseOption{virtual ~BaseOption()=default;virtual void load(){};virtual void save()const{};virtual void reset(){};};
inline std::vector<BaseOption*>& registry(){static std::vector<BaseOption*> v;return v;}
template<class T,bool PerGame=true>class Option:public BaseOption {
 T value_,default_;std::string key_;
public:Option(std::string key,T initial=T(),std::string="config"):value_(initial),default_(initial),key_(key){registry().push_back(this);}
 T& get(){return value_;}const T& get()const{return value_;}void set(T value){value_=value;}void reset()override{value_=default_;}
 explicit operator T()const{return value_;}operator T&(){return value_;}
 void load()override{}void save()const override{}
};
inline Option<int> VrGun{"vr.Gun",1},VrReload{"vr.Reload",0},VrGunAngle{"vr.GunAngle",0},VrReticleMode{"vr.ReticleMode",1};
inline Option<float> VrWorldScale{"vr.WorldScale",.11f},VrHudDepth{"vr.HudDepth",40},VrHudSize{"vr.HudSize",1},VrReticleMinSize{"vr.ReticleMinSize",.2f},VrReticleMaxSize{"vr.ReticleMaxSize",.45f},VrReticleShrinkDistance{"vr.ReticleShrinkDistance",6.5f};
inline Option<bool> VrLaser{"vr.Laser",true},VrWiden{"vr.Widen",true},VrDualWield{"vr.DualWield",true},VrSlideReload{"vr.SlideReload",true},VrGameHands{"vr.GameHands",true};
inline Option<std::string> VrHaptics{"vr.Haptics","medium"},VrGunFile{"vr.GunFile",""},VrTwoPlayerP1Hand{"vr.TwoPlayerP1Hand","left"};
}
#include "reticle_settings.h"
#include "hud_size_settings.h"
