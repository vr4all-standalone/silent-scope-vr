// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "menu_model.h"
#include <limits>

namespace silent_scope {
struct MenuHandInput {
    bool tracked{},back{},select{},trigger{}; // B/Y, A/X, trigger
    double stick_x{},stick_y{};
    int pointer_row{-1};double pointer_u{.5};
};
struct MenuFrame {
    double time{}; // monotonic seconds
    bool focused{true},recentered{},menu_action{};
    std::array<MenuHandInput,2> hands{};
};
struct MenuEffects {
    bool save{},recenter{},service{},quit{},release_inputs{},gameplay_allowed{};
};
class MenuController {
public:
    bool open() const{return open_;}
    bool how_to_play() const{return help_;}
    bool quit_confirmation() const{return quit_confirmation_;}
    MenuRow selected_row() const{return selected_;}
    // Integrator must route no native buttons unless gameplay_allowed, and
    // release native/RifleInput state whenever release_inputs is emitted.
    MenuEffects update(const MenuFrame& frame,Settings& settings,MenuCapabilities capabilities={}) {
        MenuEffects effects;
        const bool time_valid=std::isfinite(frame.time)&&frame.time>=0&&(!has_time_||frame.time>=last_time_);
        if(std::isfinite(frame.time)){last_time_=frame.time;has_time_=true;}else has_time_=false;
        const bool hand_changed=settings.active_hand!=hand_;
        hand_=settings.active_hand;
        const bool active_valid=hand_>=0&&hand_<2&&frame.hands[hand_].tracked;
        const bool active_tracking_lost=active_tracked_&&!active_valid;
        active_tracked_=active_valid;
        const bool any_tracked=frame.hands[0].tracked||frame.hands[1].tracked;
        const bool interrupted=!frame.focused||!time_valid||frame.recentered||hand_changed||!any_tracked;
        const bool neutral=!frame.menu_action&&all_buttons_released(frame);
        if(interrupted) {
            effects.release_inputs=true;blocked_=true;quit_confirmation_=false;
            cancel_inputs();remember(frame);return effects;
        }
        if(active_tracking_lost) {
            effects.release_inputs=true;blocked_=true;quit_confirmation_=false;
            // Cancel gestures from before the loss while retaining the tracked
            // offhand's ability to use a fresh Menu action immediately.
            for(int i=0;i<2;++i){hands_[i]={};hands_[i].previous=frame.hands[i];}
        }
        bool changed_menu=false,selection_consumed=false;
        if(frame.menu_action&&!previous_menu_&&menu_ready_) {
            open_=!open_;help_=quit_confirmation_=false;selected_=MenuRow::Resume;
            effects.release_inputs=true;blocked_=true;changed_menu=true;
            // A B/Y hold crossing a Menu action must not close it on release.
            for(auto& hand:hands_)hand.hold_consumed=true;
        }
        if(!frame.menu_action)menu_ready_=true;
        for(int i=0;i<2;++i) {
            auto& state=hands_[i];const auto& input=frame.hands[i];
            if(!input.tracked){state={};continue;}
            if(!input.back&&!input.select&&!input.trigger)state.ready=true;
            if(!state.ready)continue;
            if(input.back&&!state.previous.back){state.holding=true;state.hold_start=frame.time;state.hold_consumed=false;}
            const bool released_back=!input.back&&state.previous.back&&state.holding;
            if((input.back||released_back)&&state.holding&&!state.hold_consumed&&frame.time-state.hold_start>=.8) {
                state.hold_consumed=true;
                if(!open_) {open_=true;selected_=MenuRow::Resume;help_=quit_confirmation_=false;effects.release_inputs=true;blocked_=true;changed_menu=true;}
            }
            if(released_back) {
                if(open_&&!state.hold_consumed&&!changed_menu) {
                    if(quit_confirmation_){quit_confirmation_=false;changed_menu=true;}
                    else if(help_){help_=false;changed_menu=true;}
                    else {open_=false;effects.release_inputs=true;blocked_=true;changed_menu=true;}
                }
                state.holding=false;state.hold_consumed=false;
            }
            if(!open_||changed_menu)continue;
            const bool pointer_valid=input.pointer_row>=0&&input.pointer_row<menu_row_count&&std::isfinite(input.pointer_u)&&input.pointer_u>=0&&input.pointer_u<=1;
            const bool select=input.select&&!state.previous.select;
            const bool trigger=input.trigger&&!state.previous.trigger&&pointer_valid;
            if(help_) {
                // The help panel exposes one Back action. Global row navigation
                // and hidden actions remain inaccessible until it is dismissed.
                selected_=MenuRow::HowToPlay;state.axis_x=state.axis_y=0;
                if(!selection_consumed&&(select||(trigger&&input.pointer_row==int(MenuRow::HowToPlay)))) {
                    help_=false;selection_consumed=true;changed_menu=true;
                }
                continue;
            }
            const int x=axis(input.stick_x),y=axis(-input.stick_y);
            bool moved=false;
            if(y && (y!=state.axis_y||frame.time>=state.repeat_at)) {
                selected_=MenuRow((int(selected_)+y+menu_row_count)%menu_row_count);
                state.repeat_at=frame.time+(y!=state.axis_y?.35:.18);quit_confirmation_=false;moved=true;
            }
            if(x && (x!=state.axis_x||frame.time>=state.adjust_at)) {
                const auto row=selected_;quit_confirmation_=false;
                if(adjust_menu_setting(settings,row,x,capabilities)){effects.save=true;if(row==MenuRow::Hand)effects.release_inputs=true;}
                state.adjust_at=frame.time+(x!=state.axis_x?.35:.18);moved=true;
            }
            state.axis_x=x;state.axis_y=y;
            if((select||trigger)&&!moved&&!selection_consumed) {
                selection_consumed=true;
                if(pointer_valid)selected_=MenuRow(input.pointer_row);
                activate(settings,capabilities,pointer_valid&&input.pointer_u<.5?-1:1,effects);
                if(!open_||effects.recenter)changed_menu=true;
            }
        }
        if(effects.release_inputs){blocked_=true;if(settings.active_hand!=hand_){cancel_inputs();hand_=settings.active_hand;}}
        if(open_||!active_valid){blocked_=true;effects.gameplay_allowed=false;}
        else if(blocked_){if(neutral)blocked_=false;effects.gameplay_allowed=false;}
        else effects.gameplay_allowed=true;
        remember(frame);return effects;
    }
private:
    struct HandState {
        MenuHandInput previous{};bool ready{},holding{},hold_consumed{};
        double hold_start{},repeat_at{},adjust_at{};int axis_x{},axis_y{};
    };
    std::array<HandState,2> hands_{};
    bool open_{},help_{},quit_confirmation_{},blocked_{true},previous_menu_{},menu_ready_{};
    bool has_time_{},active_tracked_{};double last_time_{};int hand_{1};MenuRow selected_{MenuRow::Resume};
    static int axis(double value){return std::isfinite(value)?(value>.6?1:(value<-.6?-1:0)):0;}
    static bool all_buttons_released(const MenuFrame& f) {
        for(const auto& hand:f.hands)if(hand.tracked&&(hand.back||hand.select||hand.trigger))return false;
        return true;
    }
    void cancel_inputs(){for(auto& state:hands_)state={};menu_ready_=false;}
    void remember(const MenuFrame& f){for(int i=0;i<2;++i)hands_[i].previous=f.hands[i];previous_menu_=f.menu_action;}
    void activate(Settings& settings,MenuCapabilities cap,int direction,MenuEffects& effects) {
        const auto row=selected_;
        if(row!=MenuRow::Quit)quit_confirmation_=false;
        if(!menu_items(settings,cap)[int(row)].enabled)return;
        switch(row) {
        case MenuRow::Resume:open_=false;help_=false;effects.release_inputs=true;break;
        case MenuRow::Recenter:effects.recenter=effects.release_inputs=true;cancel_inputs();break;
        case MenuRow::HowToPlay:help_=!help_;break;
        case MenuRow::Service:open_=help_=false;effects.service=effects.release_inputs=true;break;
        case MenuRow::ResetDefaults:settings.reset();help_=false;effects.save=effects.release_inputs=true;break;
        case MenuRow::Quit:
            if(quit_confirmation_){effects.quit=effects.release_inputs=true;quit_confirmation_=false;open_=help_=false;}
            else quit_confirmation_=true;
            break;
        default:
            if(adjust_menu_setting(settings,row,direction,cap)){effects.save=true;if(row==MenuRow::Hand)effects.release_inputs=true;}
            break;
        }
    }
};
}
