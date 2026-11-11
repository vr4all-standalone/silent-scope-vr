// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
namespace silent_scope { namespace dc_mouse {
struct MenuStickSample {bool queried{},active{};float x{},y{};};
class MenuStickDirection {
    bool armed_{};int held_{},hand_{-1};
public:
    void reset(){armed_=false;held_=0;hand_=-1;}
    int update_sample(MenuStickSample sample,bool available,int hand){
        const bool valid=sample.queried&&sample.active&&std::isfinite(sample.x)&&std::isfinite(sample.y)&&
            std::abs(sample.x)<=1&&std::abs(sample.y)<=1;
        return update(sample.y,available&&valid,hand);
    }
    int update(float y,bool available,int hand){
        if(!available||hand<0||hand>1||!std::isfinite(y)){reset();return 0;}
        if(hand!=hand_){reset();hand_=hand;}
        if(std::abs(y)<=.3f){armed_=true;held_=0;return 0;}
        if(!armed_)return 0;
        if(y>=.6f)held_=-1;else if(y<=-.6f)held_=1;
        else if((held_<0&&y<0)||(held_>0&&y>0))held_=0;
        return held_;
    }
};
}}
