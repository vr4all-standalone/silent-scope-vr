#include <iostream>
#if __has_include("menu_presentation.h")
#include "menu_presentation.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#define HAS_PRESENTATION 1
#endif
#define REQUIRE(x) do { if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;} } while(false)
#ifdef HAS_PRESENTATION
using namespace silent_scope;
struct MenuRig {
    Settings settings;MenuController controller;MenuFrame frame;MenuCapabilities capabilities{true,true,true};
    MenuPresentation ui;
    MenuRig(){frame.hands[0].tracked=frame.hands[1].tracked=true;step();frame.menu_action=true;step();frame.menu_action=false;step();}
    void step(){frame.time+=.01;controller.update(frame,settings,capabilities);ui.update(settings,capabilities,controller);}
    void click(MenuRow row){frame.hands[1].pointer_row=int(row);frame.hands[1].trigger=true;step();frame.hands[1].trigger=false;step();}
};
#endif
int main(){
#ifndef HAS_PRESENTATION
    REQUIRE(false && "Silent Scope menu presentation not implemented");
#else
    MenuRig rig;const auto model=glm::translate(glm::mat4(1),glm::vec3(0,0,-1.2));
    REQUIRE(rig.ui.panel().rows.size()==11 && rig.ui.page()==0 && rig.ui.page_count()==2);
    REQUIRE(rig.ui.panel().rows[0].label=="Resume" && rig.ui.panel().rows[2].dim);
    auto ray_to=[&](float x,float y){return rig.ui.pointer(model,{0,0,0},glm::normalize(glm::vec3(x,y,-1.2f)));};
    auto layout=vr::xr::rowsLayout(rig.ui.panel());
    auto hit=ray_to(layout.width*.3f,layout.topRow-layout.rowPitch*3);
    REQUIRE(hit.row==int(MenuRow::GunAngle) && hit.u>.79 && hit.u<.81 && hit.page_delta==0);
    REQUIRE(ray_to(0,layout.height*.49f).row==-1);
    REQUIRE(rig.ui.pointer(model,{0,0,-2},{0,0,1}).row==-1);
    REQUIRE(rig.ui.pointer(glm::mat4(0),{0,0,0},{0,0,-1}).row==-1);
    REQUIRE(rig.ui.pointer(model,{0,0,0},{std::numeric_limits<float>::quiet_NaN(),0,-1}).row==-1);
    const auto moved=glm::translate(glm::mat4(1),glm::vec3(.3f,.1f,-1.2f))*glm::rotate(glm::mat4(1),.2f,glm::vec3(0,1,0));
    const auto moved_target=glm::vec3(moved*glm::vec4(0,layout.topRow-layout.rowPitch*3,0,1));
    REQUIRE(rig.ui.pointer(moved,{0,0,0},glm::normalize(moved_target)).row==int(MenuRow::GunAngle));
    auto footer=ray_to(layout.width*.25f,-layout.height*.47f);REQUIRE(footer.row==-1 && footer.page_delta==1);
    rig.ui.change_page(footer.page_delta);rig.ui.update(rig.settings,rig.capabilities,rig.controller);
    REQUIRE(rig.ui.page()==1 && rig.ui.panel().rows.front().label=="Reticle shrink distance");
    hit=ray_to(0,layout.topRow);REQUIRE(hit.row==int(MenuRow::ReticleShrinkDistance));
    hit=ray_to(0,layout.topRow-layout.rowPitch*10);REQUIRE(hit.row==int(MenuRow::Quit));
    rig.click(MenuRow::Quit);REQUIRE(rig.ui.page()==1 && rig.ui.panel().rows.back().value=="Confirm quit");
    REQUIRE(rig.ui.panel().title.find("Confirm")!=std::string::npos);
    REQUIRE(ray_to(layout.width*.25f,-layout.height*.47f).page_delta==0);
    rig.click(MenuRow::HowToPlay);REQUIRE(rig.ui.panel().title.find("How to play")!=std::string::npos);
    layout=vr::xr::rowsLayout(rig.ui.panel());REQUIRE(rig.ui.panel().rows.size()<=11);
    REQUIRE(ray_to(0,layout.topRow).row==-1);
    hit=ray_to(0,layout.topRow-layout.rowPitch*(rig.ui.panel().rows.size()-1));REQUIRE(hit.row==int(MenuRow::HowToPlay));
    rig.click(MenuRow::HowToPlay);rig.click(MenuRow::Resume);REQUIRE(!rig.controller.open());
    REQUIRE(ray_to(0,0).row==-1 && ray_to(0,0).page_delta==0);
    rig.ui.status_fps(72,21.4);rig.settings.fps_counter=true;rig.step();
    REQUIRE(rig.ui.status_text().find("VR 72")!=std::string::npos && rig.ui.status_text().find("Native 21")!=std::string::npos);
    rig.ui.status_fps(std::numeric_limits<double>::infinity(),-3);REQUIRE(rig.ui.status_text().find("--")!=std::string::npos);
    rig.ui.save_error("Cannot replace settings\nfile");REQUIRE(rig.ui.status_text().find("not saved")!=std::string::npos && rig.ui.status_text().find('\n')==std::string::npos);
    rig.ui.save_error("");rig.settings.fps_counter=false;rig.step();REQUIRE(rig.ui.status_text().empty());
    rig.frame.menu_action=true;rig.step();rig.frame.menu_action=false;rig.step();rig.click(MenuRow::WorldSize);
    REQUIRE(rig.ui.page()==1);rig.ui.change_page(-1);rig.step();REQUIRE(rig.ui.page()==0);
    rig.frame.hands[1].pointer_row=-1;rig.frame.hands[1].stick_y=-1;rig.step();rig.frame.hands[1].stick_y=0;rig.step();
    REQUIRE(rig.ui.page()==1 && rig.controller.selected_row()==MenuRow::PanelDistance);
    rig.click(MenuRow::Resume);

    GLContext context;GLuint target{},colour{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
    glGenTextures(1,&colour);glBindTexture(GL_TEXTURE_2D,colour);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,512,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    const auto vp=glm::ortho(-.6f,.6f,-.6f,.6f,.1f,5.f);
    auto draw=[&](){glViewport(0,0,512,512);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);rig.ui.draw(vp,model);std::vector<unsigned char> pixels(512*512*4);glReadPixels(0,0,512,512,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
    auto empty=draw();rig.frame.menu_action=true;rig.step();rig.frame.menu_action=false;rig.step();auto menu=draw();
    REQUIRE(menu!=empty);int purple=0,letters=0;
    for(std::size_t i=0;i<menu.size();i+=4){purple+=menu[i]>90&&menu[i+2]>menu[i]+15;letters+=menu[i]>200&&menu[i+1]>200&&menu[i+2]>200;}
    REQUIRE(purple>500 && letters>500);
    rig.ui.change_page(1);auto second_page=draw();REQUIRE(second_page!=menu);
    rig.click(MenuRow::HowToPlay);auto help=draw();REQUIRE(help!=second_page);
    rig.click(MenuRow::HowToPlay);rig.click(MenuRow::Quit);auto confirm=draw();REQUIRE(confirm!=second_page);
    rig.click(MenuRow::Resume);rig.settings.fps_counter=true;rig.ui.status_fps(72,22);rig.step();auto fps=draw();REQUIRE(fps!=empty);
    rig.ui.save_error("Cannot write settings");REQUIRE(draw()!=fps);rig.ui.save_error("");rig.settings.fps_counter=false;rig.step();REQUIRE(draw()==empty);
    REQUIRE(glGetError()==GL_NO_ERROR);vr::xr::termPanel();glDeleteTextures(1,&colour);glDeleteFramebuffers(1,&target);
#endif
    std::cout<<"Menu pages, ray mapping, help/Quit, FPS/error text and production GPU rendering passed\n";
}
