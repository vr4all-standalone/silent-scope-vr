// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
namespace silent_scope {
struct CabinetButtons {bool menu{},start{},secondary{},recenter{};};
struct CabinetControlsOutput {bool menu_open{},quit{},recenter{},resumed{},coin{},start{};};
class CabinetControls {
    CabinetButtons previous_{true,true,true,true};
    bool menu_open_{};
    double sequence_start_{-1},previous_time_{-1};
public:
    void suspend(){previous_={true,true,true,true};sequence_start_=-1;previous_time_=-1;}
    CabinetControlsOutput update(double now,bool enabled,CabinetButtons buttons){
        CabinetControlsOutput out;out.menu_open=menu_open_;
        if(!std::isfinite(now)||now<0||now<previous_time_){suspend();return out;}
        previous_time_=now;
        if(!enabled){sequence_start_=-1;previous_=buttons;return out;}
        bool menu_pressed=buttons.menu&&!previous_.menu,start_pressed=buttons.start&&!previous_.start;
        bool was_menu=menu_open_;
        if(menu_pressed){menu_open_=!menu_open_;out.resumed=!menu_open_;sequence_start_=-1;}
        if(menu_open_&&buttons.secondary&&!previous_.secondary)out.quit=true;
        if(menu_open_&&start_pressed){menu_open_=false;out.resumed=true;sequence_start_=-1;}
        out.recenter=buttons.recenter&&!previous_.recenter;
        // The whole menu interaction consumes the button, including the frame that resumes.
        if(!was_menu&&!menu_open_&&!out.resumed&&!menu_pressed&&start_pressed&&sequence_start_<0)sequence_start_=now;
        if(menu_open_||out.resumed)sequence_start_=-1;
        if(sequence_start_>=0){double elapsed=now-sequence_start_;
            out.coin=elapsed<.1||(elapsed>=.2&&elapsed<.3)||(elapsed>=.4&&elapsed<.5);
            out.start=elapsed>=.7&&elapsed<.85;
            if(elapsed>=.9)sequence_start_=-1;
        }
        out.menu_open=menu_open_;previous_=buttons;return out;
    }
};
}
