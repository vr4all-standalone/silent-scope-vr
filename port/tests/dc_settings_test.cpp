#include "dreamcast/dc_settings.h"
#include "cfg/option.h"
#include "emulator.h"
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    namespace dc=silent_scope::dc_settings;
    const auto smoothing=dc::Setting::AimSmoothing;
    settings.content.gameId="T9507N";config::values.clear();dc::sanitize_loaded();
    require(dc::value(smoothing)=="Low","Missing smoothing preference must default to Low");
    require(dc::change(smoothing,1)&&dc::value(smoothing)=="Medium","Smoothing step failed");
    require(config::loadStr("config","vr.RifleSmoothing")=="2","Smoothing persistence key/value failed");
    dc::sanitize_loaded();require(dc::value(smoothing)=="Medium","Explicit smoothing lost on reload");
    for(const auto* invalid:{"-1","4","1.5","nan","2junk","garbage"}){
        config::values["config:vr.RifleSmoothing"]=invalid;dc::sanitize_loaded();
        require(dc::value(smoothing)=="Low","Invalid smoothing must default to Low");
    }
    for(const auto* choice:{"0","1","2","3"}){
        config::values["config:vr.RifleSmoothing"]=choice;dc::sanitize_loaded();
        const auto before=dc::value(smoothing);dc::sanitize_loaded();require(dc::value(smoothing)==before,"Saved choice changed on reload");
    }
    require(!dc::change(smoothing,1),"Smoothing upper bound must clamp");
    dc::reset_defaults();require(dc::value(smoothing)=="Low"&&config::loadStr("config","vr.RifleSmoothing")=="1","Smoothing Reset failed");
    const auto zoom=dc::Setting::ScopeZoom;
    config::values["config:vr.ScopeZoom"]="2";
    dc::sanitize_loaded();
    dc::reset_scope_zoom();
    require(dc::scope_stability_level()==1&&dc::scope_magnification()==1,"Legacy zoom overrode 1x at game start");
    const auto zoomWrites=config::writes;
    require(dc::change(zoom,-1)&&dc::scope_magnification()==.5f,"0.5x step failed");
    require(dc::change(zoom,-1)&&dc::scope_magnification()==.2f&&!dc::change(zoom,-1),"0.2x step or bound failed");
    dc::sanitize_loaded();
    require(dc::value(zoom)=="0.2x","Menu refresh reset the active zoom");
    require(config::writes==zoomWrites,"Session zoom wrote a saved setting");
    dc::reset_scope_zoom();
    require(dc::value(zoom)=="1x","A new session kept 0.2x");
    require(dc::change(zoom,-1)&&dc::change(zoom,-1),"Zoom out failed after restarting");
    require(dc::change(zoom,1)&&dc::change(zoom,1)&&dc::scope_magnification()==1,"Return to 1x failed");
    require(dc::change(zoom,1)&&dc::scope_magnification()==2,"2x step failed");
    require(dc::change(zoom,1)&&dc::scope_magnification()==5&&!dc::change(zoom,1),"5x step or bound failed");
    dc::reset_scope_zoom();
    require(dc::value(zoom)=="1x","A new session kept 5x");
    require(dc::change(dc::Setting::ScopeStability,-1),"Scope stability toggle failed");
    dc::sanitize_loaded();
    require(dc::scope_stability_level()==0&&dc::scope_magnification()==1,"Scope settings lost on reload");
    for(const auto* level:{"1","2","3","4","5"})
        require(dc::change(dc::Setting::ScopeStability,1)&&dc::value(dc::Setting::ScopeStability)==level,"Stability step failed");
    dc::sanitize_loaded();
    require(dc::scope_stability_level()==5&&!dc::change(dc::Setting::ScopeStability,1),"Level 5 stability persistence or bound failed");
    config::values.erase("config:vr.ScopeStabilityLevel");
    config::values["config:vr.ScopeStability"]="no";
    dc::sanitize_loaded();
    require(dc::scope_stability_level()==0&&dc::scope_magnification()==1,"Legacy stability migration failed");
    dc::reset_defaults();
    require(dc::scope_stability_level()==1&&dc::scope_magnification()==1,"Scope reset failed");
    const auto depth=dc::Setting::ScopeImageDepth, size=dc::Setting::ScopeImageSize;
    require(dc::change(depth,-1)&&dc::change(size,1),"Scope image controls did not step");
    dc::sanitize_loaded();
    require(dc::value(depth)=="28 mm forward"&&dc::value(size)=="130%","Scope image fit did not survive reload");
    const auto rifle=dc::Setting::RifleMode;
    config::values.erase("config:vr.AwpScopeImageDepthMm");
    config::values.erase("config:vr.AwpScopeImageSizePercent");
    dc::sanitize_loaded();
    require(dc::change(rifle,1)&&dc::value(depth)=="8 mm forward"&&dc::value(size)=="110%","Simulation fit default failed");
    require(dc::change(depth,1)&&dc::change(size,1),"Simulation fit did not step");
    dc::sanitize_loaded();
    require(dc::value(depth)=="7 mm forward"&&dc::value(size)=="115%","Simulation fit did not survive reload");
    require(dc::change(rifle,-1),"Arcade selection failed");
    require(dc::value(depth)=="28 mm forward"&&dc::value(size)=="130%","Simulation fit overwrote Arcade fit");
    for(int i=0;i<60;++i){dc::change(depth,1);dc::change(size,1);}
    require(dc::value(depth)=="30 mm rear"&&dc::value(size)=="150%","Scope image bounds failed");
    dc::reset_defaults();
    require(dc::value(depth)=="27 mm forward"&&dc::value(size)=="125%","Scope image fit reset failed");
    require(dc::change(rifle,1)&&dc::value(depth)=="8 mm forward"&&dc::value(size)=="110%","Simulation fit reset failed");
    dc::reset_defaults();
    const auto gunSize=dc::Setting::GunSize, gunMode=dc::Setting::RifleMode;
    require(dc::rifle_model_scale()==1.f,"Arcade size baseline changed");
    dc::change(gunSize,1);dc::change(gunMode,1);
    require(std::abs(dc::rifle_model_scale()-1.1f)<.0001f,"AWP baseline changed");
    for(int i=0;i<20;++i)dc::change(gunSize,1);
    dc::sanitize_loaded();
    require(dc::value(gunSize)=="160%"&&std::abs(dc::rifle_model_scale()-1.76f)<.0001f,"AWP maximum did not persist");
    require(std::abs(dc::rifle_model_scale(false)-1.05f)<.0001f&&std::abs(dc::rifle_model_scale(true)-1.76f)<.0001f,
        "Holstered rifle scale followed the equipped model instead of its own saved size");
    require(dc::simulation_mode(),"Reading the inactive holster scale changed the selected rifle");
    dc::change(gunMode,-1);
    require(dc::value(gunSize)=="105%","AWP size overwrote Arcade size");
    for(int i=0;i<30;++i)dc::change(gunSize,-1);
    require(dc::value(gunSize)=="40%","Gun size minimum failed");
    dc::reset_defaults();
    const auto laser=dc::Setting::Laser, wider=dc::Setting::WiderView;
    require(dc::value(laser)=="Normal"&&dc::laser_half_width()==.0016f,"Normal laser baseline failed");
    require(dc::change(laser,1)&&dc::value(laser)=="5x","5x laser step failed");
    require(dc::change(laser,1)&&dc::value(laser)=="10x","10x laser step failed");
    dc::sanitize_loaded();
    require(dc::value(laser)=="10x","Laser width did not persist");
    require(dc::select_laser_choice(0),"Explicit laser Off failed");
    dc::sanitize_loaded();
    require(dc::select_laser_choice(3)&&dc::value(laser)=="10x","Explicit Off lost the tuned beam width");

    require(dc::toggle_aiming_aids()&&dc::value(laser)=="Off"&&dc::value(dc::Setting::Reticle)=="None","Paired shortcut did not hide both aids");
    dc::sanitize_loaded();
    require(dc::toggle_aiming_aids()&&dc::value(laser)=="10x"&&dc::value(dc::Setting::Reticle)=="Custom","Paired shortcut lost width or save/load");
    dc::set_runtime_capabilities({true,true,10});
    require(dc::change(wider,1)&&dc::wider_view_mode()==2,"Fade selection failed");
    dc::sanitize_loaded();
    require(dc::wider_view_mode()==2&&config::VrWiden.get(),"Fade did not persist");
    dc::change(wider,-1);dc::change(wider,-1);
    dc::sanitize_loaded();
    require(dc::wider_view_mode()==0&&!config::VrWiden.get(),"Original view migration failed");
    config::values.erase("config:vr.WidenMode");config::values["config:vr.Widen"]="no";
    dc::sanitize_loaded();
    require(dc::wider_view_mode()==0,"Legacy Wider Off was reset");
    settings.content.gameId="OTHER";const auto writes=config::writes;
    require(!dc::change(smoothing,1)&&config::writes==writes,"Smoothing leaked to other game");
    std::cout<<"Aim smoothing settings: defaults, strict load, all choices, persistence, bounds, Reset and title gate pass\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
