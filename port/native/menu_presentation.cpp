// SPDX-License-Identifier: GPL-2.0-or-later
#include "menu_presentation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace silent_scope {
namespace {
bool stepper(MenuRow row) {
    switch(row) {
    case MenuRow::GunAngle:case MenuRow::Hand:case MenuRow::Laser:case MenuRow::Reticle:
    case MenuRow::ReticleMinSize:case MenuRow::ReticleMaxSize:case MenuRow::ReticleShrinkDistance:
    case MenuRow::Haptics:case MenuRow::WorldSize:case MenuRow::PanelDistance:
    case MenuRow::HudSize:case MenuRow::WiderView:case MenuRow::FpsCounter:return true;
    default:return false;
    }
}
bool finite(glm::vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);}
bool finite(const glm::mat4& value){for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(!std::isfinite(value[c][r]))return false;return true;}
std::string rate(double value){return std::isfinite(value)&&value>0&&value<10000?menu_number(value):"--";}
}
void MenuPresentation::update(const Settings& settings,MenuCapabilities capabilities,const MenuController& controller) {
    const bool was_open=open_;open_=controller.open();help_=controller.how_to_play();confirm_=controller.quit_confirmation();
    selected_=controller.selected_row();const int selected=int(selected_);
    if((!was_open&&open_)||selected!=last_selected_)page_=std::clamp(selected/rows_per_page,0,page_count()-1);
    last_selected_=selected;items_=menu_items(settings,capabilities);fps_enabled_=settings.fps_counter;
    rebuild_panel();rebuild_status();
}
void MenuPresentation::change_page(int delta) {
    if(!open_||help_||confirm_||delta==0)return;
    page_=std::clamp(page_+(delta<0?-1:1),0,page_count()-1);rebuild_panel();
}
void MenuPresentation::rebuild_panel() {
    global_rows_.fill(-1);panel_.rows.clear();
    if(help_) {
        panel_.title="Silent Scope - How to play";
        panel_.rows={
            {"Aim / Scope","Look through tracked rifle lens",false,true,false},
            {"Fire / Reload","Trigger / Automatic",false,true,false},
            {"Credits / Start","A / X",false,true,false},
            {"Support","Grip near rifle fore-end",false,true,false},
            {"Open settings","Menu / hold B / Y 0.8s",false,true,false},
            {"Choose","Point + Trigger / A / X",false,true,false},
            {"Navigate / adjust","Thumbstick",false,true,false},
            {"Return","Short B / Y",false,true,false},
            {"Stick clicks","Left: Recenter / Right: Laser",false,true,false},
            {"Service","Trigger: Next / A-X: Select",false,true,false},
            {"Back to settings","",true,false,false}};
        global_rows_[panel_.rows.size()-1]=int(MenuRow::HowToPlay);
        panel_.footer="Service: GAME MODE returns to play | B / Y: Back";
    } else {
        panel_.title=confirm_?"Silent Scope - Confirm quit":"Silent Scope - VR4ALL";
        for(int index=page_*rows_per_page;index<std::min((page_+1)*rows_per_page,menu_row_count);++index) {
            const auto& item=items_[index];global_rows_[panel_.rows.size()]=index;
            panel_.rows.push_back({item.label,item.row==MenuRow::Quit&&confirm_?"Confirm quit":item.value,
                                  item.row==selected_,!item.enabled,item.enabled&&stepper(item.row)});
        }
        panel_.footer=confirm_?"Select Quit again to exit | B / Y: Cancel":
            "Page "+std::to_string(page_+1)+" / "+std::to_string(page_count())+"   < Previous | Next >   B / Y: Back";
    }
    if(!error_.empty())panel_.footer="Settings not saved: "+error_;
}
void MenuPresentation::rebuild_status() {
    status_=fps_enabled_?"VR "+rate(headset_hz_)+" FPS | Native "+rate(native_hz_)+" FPS":"";
    if(!error_.empty()){if(!status_.empty())status_+=" | ";status_+="Settings not saved";}
}
void MenuPresentation::status_fps(double headset_hz,double native_hz) {
    headset_hz_=headset_hz;native_hz_=native_hz;rebuild_status();
}
void MenuPresentation::save_error(const std::string& message) {
    error_.clear();for(unsigned char ch:message){if(error_.size()>=65)break;error_+=ch>=32&&ch<127?char(ch):' ';}
    rebuild_panel();rebuild_status();
}
MenuPointer MenuPresentation::pointer(const glm::mat4& model,glm::vec3 origin,glm::vec3 direction) const {
    MenuPointer result;
    if(!open_||!finite(model)||!finite(origin)||!finite(direction)||glm::dot(direction,direction)<1e-12f||std::abs(glm::determinant(model))<1e-8f)return result;
    glm::vec2 uv;
    if(!vr::xr::rowsPanelHit(model,panel_,origin,direction,&result.point,&uv))return result;
    result.u=uv.x;const int local=vr::xr::rowAt(model,panel_,origin,direction,nullptr);
    if(local>=0&&local<int(panel_.rows.size()))result.row=global_rows_[local];
    else if(!help_&&!confirm_&&error_.empty()) {
        const auto layout=vr::xr::rowsLayout(panel_);
        // Rows occupy their exact shared-layout positions; the remaining band
        // beneath the last row is the footer, never a gameplay/menu row.
        const float local_y=(.5f-uv.y)*layout.height;
        const float row_bottom=layout.topRow-layout.rowPitch*(float(panel_.rows.size())-.5f);
        if(local_y<row_bottom) {
            if(uv.x<.45f&&page_>0)result.page_delta=-1;
            else if(uv.x>.55f&&page_+1<page_count())result.page_delta=1;
        }
    }
    return result;
}
void MenuPresentation::draw(const glm::mat4& view_projection,const glm::mat4& model) const {
    if(open_)vr::xr::drawRowsPanel(view_projection,model,panel_);
    if(status_.empty())return;
    int width{},height{};const auto texture=vr::xr::statusTexture(status_,width,height);
    if(!texture||width<=0||height<=0)return;
    float physical_height=.038f,physical_width=physical_height*float(width)/height;
    if(physical_width>.82f){physical_height*=.82f/physical_width;physical_width=.82f;}
    const float y=open_?vr::xr::rowsLayout(panel_).height*.5f+.045f:.47f;
    const auto status_model=glm::translate(model,glm::vec3(0,y,.002f));
    vr::xr::drawTexture(view_projection,status_model,texture,physical_width*.5f,physical_height*.5f);
}
}
