// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rifle_input.h"
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace silent_scope {
enum class Reticle { Custom, None };
enum class Haptics { Off, Light, Medium, Strong };
struct Settings {
    int active_hand{1};
    double panel_distance{1.8},gun_angle{};
    bool laser_on{true},wider_view{true},fps_counter{};
    Reticle reticle{Reticle::Custom};
    Haptics haptics{Haptics::Medium};
    double reticle_min_size{.25},reticle_max_size{1},reticle_shrink_distance{6};
    double world_size{.1},hud_size{1};
    Calibration calibration;
    // Reset VR preferences, never the accepted cabinet/aim calibration.
    void reset() {auto preserved=calibration;*this=Settings{};calibration=preserved;}
    void save(std::ostream& out) const {
        out<<std::setprecision(17)<<"hand="<<active_hand<<"\npanel_distance="<<panel_distance
           <<"\ngun_angle="<<gun_angle<<"\nyaw_center="<<calibration.yaw_center
           <<"\npitch_center="<<calibration.pitch_center<<"\nyaw_half_range="<<calibration.yaw_half_range
           <<"\npitch_half_range="<<calibration.pitch_half_range
           <<"\nlaser_on="<<int(laser_on)<<"\nwider_view="<<int(wider_view)<<"\nfps_counter="<<int(fps_counter)
           <<"\nreticle="<<int(reticle)<<"\nhaptics="<<int(haptics)
           <<"\nreticle_min_size="<<reticle_min_size<<"\nreticle_max_size="<<reticle_max_size
           <<"\nreticle_shrink_distance="<<reticle_shrink_distance<<"\nworld_size="<<world_size<<"\nhud_size="<<hud_size<<'\n';
    }
    static Settings load(std::istream& in) {
        Settings settings;
        std::string line;
        while(std::getline(in,line)) {
            if(line.size()>512)continue;
            auto separator=line.find('=');if(separator==std::string::npos)continue;
            auto key=line.substr(0,separator);std::istringstream value(line.substr(separator+1));
            double parsed{};
            if(!(value>>parsed)||!std::isfinite(parsed))continue;
            value>>std::ws;if(!value.eof())continue;
            auto assign=[&](double& target,double minimum,double maximum){if(parsed>=minimum&&parsed<=maximum)target=parsed;};
            if(key=="hand") {if(parsed==0||parsed==1)settings.active_hand=int(parsed);}
            else if(key=="panel_distance")assign(settings.panel_distance,.5,10);
            else if(key=="gun_angle")assign(settings.gun_angle,-30,30);
            else if(key=="yaw_center")assign(settings.calibration.yaw_center,-180,180);
            else if(key=="pitch_center")assign(settings.calibration.pitch_center,-90,90);
            else if(key=="yaw_half_range")assign(settings.calibration.yaw_half_range,1,180);
            else if(key=="pitch_half_range")assign(settings.calibration.pitch_half_range,1,90);
            else if(key=="laser_on") {if(parsed==0||parsed==1)settings.laser_on=parsed==1;}
            else if(key=="wider_view") {if(parsed==0||parsed==1)settings.wider_view=parsed==1;}
            else if(key=="fps_counter") {if(parsed==0||parsed==1)settings.fps_counter=parsed==1;}
            else if(key=="reticle") {if(parsed==0||parsed==1)settings.reticle=Reticle(int(parsed));}
            else if(key=="haptics") {if(parsed>=0&&parsed<=3&&std::floor(parsed)==parsed)settings.haptics=Haptics(int(parsed));}
            else if(key=="reticle_min_size")assign(settings.reticle_min_size,.05,1);
            else if(key=="reticle_max_size")assign(settings.reticle_max_size,.25,3);
            else if(key=="reticle_shrink_distance")assign(settings.reticle_shrink_distance,.2,20);
            else if(key=="world_size")assign(settings.world_size,.01,1);
            else if(key=="hud_size")assign(settings.hud_size,.5,2);
        }
        return settings;
    }
};
}
