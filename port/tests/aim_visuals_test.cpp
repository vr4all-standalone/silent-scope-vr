#include <iostream>
#if __has_include("aim_visuals.h")
#include "aim_visuals.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#define HAS_AIM_VISUALS 1
#endif
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;}}while(false)
#ifdef HAS_AIM_VISUALS
using namespace silent_scope;
std::shared_ptr<scene::RawFrame> make_scene(double depth=4,bool flip=false) {
    auto frame=std::make_shared<scene::RawFrame>();frame->width=512;frame->height=384;frame->lens={512,384,443,443,256,192};frame->camera_owned=true;
    scene::RawTriangle triangle;triangle.state.depth_mode=512|(flip?(1u<<17):0);
    const double positions[3][2]={{-2,-2},{2,-2},{0,2}};
    for(int i=0;i<3;++i){auto& v=triangle.vertex[i];v.q=scene::SceneMetresPerUnit/depth;v.x=256+443*positions[i][0]/depth;v.y=192-443*positions[i][1]/depth;if(flip)v.y=384-v.y;}
    frame->triangles.push_back(triangle);return frame;
}
#endif
int main(){
#ifndef HAS_AIM_VISUALS
    REQUIRE(false && "Native aim visuals not implemented");
#else
    Settings settings;AimVisuals visuals;auto frame=make_scene();auto model=glm::translate(glm::mat4(1),glm::vec3(.2,.1,0));
    visuals.prepare(frame,settings,model,true);REQUIRE(visuals.hit() && std::abs(visuals.hit()->point.z+4)<1e-5);
    REQUIRE(glm::length(visuals.beam_origin()-glm::vec3(.2,.105,-.94))<1e-6f);
    const auto original=visuals.hit()->point;
    auto wide=make_scene();wide->lens.fx/=2;wide->lens.fy/=2;wide->wider_applied=true;wide->wider_factor=2;
    for(auto& v:wide->triangles[0].vertex){v.x=256+(v.x-256)/2;v.y=192+(v.y-192)/2;}
    visuals.prepare(wide,settings,model,true);REQUIRE(visuals.hit() && glm::length(visuals.hit()->point-original)<1e-5);
    auto near=make_scene(2);near->triangles.insert(near->triangles.begin(),frame->triangles[0]);
    visuals.prepare(near,settings,model,true);REQUIRE(visuals.hit() && std::abs(visuals.hit()->point.z+2)<1e-5);
    auto up_model=glm::rotate(model,glm::radians(10.f),glm::vec3(1,0,0));visuals.prepare(frame,settings,up_model,true);
    REQUIRE(visuals.hit() && visuals.hit()->point.y>0);
    auto flip=make_scene(4,true);visuals.prepare(flip,settings,up_model,true);REQUIRE(visuals.hit() && visuals.hit()->point.y>0);
    auto overlays=make_scene();overlays->triangles[0].overlay=true;visuals.prepare(overlays,settings,model,true);REQUIRE(!visuals.hit());
    overlays=make_scene();for(auto& v:overlays->triangles[0].vertex)v.q=1/1.1;visuals.prepare(overlays,settings,model,true);REQUIRE(!visuals.hit());
    auto empty=make_scene();empty->triangles.clear();visuals.prepare(empty,settings,model,true);REQUIRE(!visuals.hit());
    auto invalid=make_scene();invalid->complete=false;visuals.prepare(invalid,settings,model,true);REQUIRE(!visuals.hit());
    invalid=make_scene();invalid->width=513;visuals.prepare(invalid,settings,model,true);REQUIRE(!visuals.hit());
    visuals.prepare(frame,settings,glm::mat4(0),true);REQUIRE(!visuals.hit());
    visuals.prepare(frame,settings,model,false);REQUIRE(!visuals.hit());
    auto moved=glm::rotate(model,glm::radians(85.f),glm::vec3(0,1,0));visuals.prepare(frame,settings,moved,true);REQUIRE(!visuals.hit());
    settings.world_size=.2;visuals.prepare(frame,settings,model,true);REQUIRE(visuals.hit() && std::abs(visuals.hit()->point.z+8)<1e-5);settings.world_size=.1;
    REQUIRE(std::abs(AimVisuals::reticle_reference(1,settings)-1.8)<1e-9);
    REQUIRE(std::abs(AimVisuals::reticle_reference(7.8,settings)-3.9)<1e-9);
    REQUIRE(std::abs(AimVisuals::reticle_reference(100,settings)-25)<1e-9);
    settings.reticle_max_size=2;REQUIRE(std::abs(AimVisuals::reticle_reference(7.8,settings)-7.8)<1e-9);settings.reticle_max_size=1;
    settings.reticle=Reticle::None;visuals.prepare(frame,settings,model,true);REQUIRE(visuals.hit() && visuals.reticle_radius()==0);
    settings.reticle=Reticle::Custom;settings.laser_on=false;visuals.prepare(frame,settings,model,true);REQUIRE(visuals.reticle_radius()>0);

    GLContext context;GLuint target{},colour{},depth{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
    glGenTextures(1,&colour);glBindTexture(GL_TEXTURE_2D,colour);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,512,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    auto vp=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f);
    auto clear=[&](){glViewport(0,0,512,512);glDepthMask(GL_TRUE);glClearDepth(1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);};
    auto pixels=[](){std::vector<unsigned char> p(512*512*4);glReadPixels(0,0,512,512,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    clear();const auto black=pixels();visuals.draw(vp,{0,0,0});const auto custom=pixels();REQUIRE(custom!=black);
    REQUIRE(custom[(256*512+256)*4]<20);int red=0;for(std::size_t i=0;i<custom.size();i+=4)red+=custom[i]>100&&custom[i+1]<60;REQUIRE(red>20);
    clear();settings.reticle=Reticle::None;visuals.prepare(frame,settings,model,true);visuals.draw(vp,{0,0,0});REQUIRE(pixels()==black);
    settings.laser_on=true;visuals.prepare(frame,settings,model,true);visuals.draw(vp,{0,0,0});REQUIRE(pixels()!=black);
    clear();glClearDepth(.01);glClear(GL_DEPTH_BUFFER_BIT);visuals.draw(vp,{0,0,0});REQUIRE(pixels()==black);
    AimVisuals::draw_menu_pointer(vp,{.2,.1,-.2},{0,0,-1.2},true);REQUIRE(pixels()!=black);
    clear();AimVisuals::draw_menu_pointer(vp,{.2,.1,-.2},{0,0,-1.2},false);REQUIRE(pixels()==black);
    visuals.prepare({},settings,model,true);visuals.draw(vp,{0,0,0});REQUIRE(pixels()==black);
    REQUIRE(glGetError()==GL_NO_ERROR);vr::xr::termPanel();glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&colour);glDeleteFramebuffers(1,&target);
#endif
    std::cout<<"Nearest native geometry aim, stock lens, pitch/Y flip, reticle sizing and GPU effects passed\n";
}
