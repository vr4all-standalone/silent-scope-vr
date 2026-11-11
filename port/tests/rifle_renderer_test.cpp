#include "rifle_renderer.h"
#include "dreamcast/dc_rifle_fit.h"
#include "gl_context.h"
#include "scene/gl_scene.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
using namespace silent_scope;
int main(int argc,char**) {
    const auto mesh=argc>1?dc_rifle::mesh():make_rifle_mesh();
    if(argc>1&&mesh.vertices.size()!=6399*3)return 1;
    GLContext context;RifleRenderer renderer(mesh);
    scene::GLScene world;
    auto frame=std::make_shared<scene::RawFrame>();frame->width=512;frame->height=384;
    frame->lens={512,384,443,443,256,192};frame->complete=true;frame->sequence=1;
    scene::RawTriangle triangle;triangle.state.depth_mode=512;triangle.state.texture_mode=0xc6;
    triangle.texture[0]=std::make_shared<const scene::TextureImage>(99,1,2,2,std::vector<uint32_t>(4,0xff555555));
    for(auto& vertex:triangle.vertex){vertex.q=.5;vertex.x=256;vertex.y=192;vertex.a=vertex.r=vertex.g=vertex.b=255;}
    frame->triangles.push_back(triangle);world.prepare(frame,false);
    GLuint target{},color{},depth{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,128,128);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 1;
    auto source=std::make_shared<RasterFrame>(RasterFrame{1,1,1,4,4,1,{}});
    for(int y=0;y<4;++y)for(int x=0;x<4;++x)source->pixels.push_back(y<2?(x<2?0xffff0000:0xff00ff00):(x<2?0xff0000ff:0xffffff00));
    const auto ocular=mesh.ocular;
    glm::vec3 eye(ocular.x,ocular.y,ocular.z+.1);
    auto projection=glm::perspective(glm::radians(32.f),1.f,.005f,5.f);
    auto draw=[&](glm::vec3 camera){
        glViewport(0,0,128,128);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        auto view=glm::lookAt(camera,glm::vec3(ocular.x,ocular.y,ocular.z),glm::vec3(0,1,0));
        // The real XR host draws the world and scope in this same context.
        world.draw(projection*view,1,1.8);
        renderer.draw_rifle(projection*view,glm::mat4(1),camera,source);
    };
    auto pixel=[](int x,int y){std::array<unsigned char,4> p{};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    draw(eye);
    auto tl=pixel(48,80),tr=pixel(80,80),bl=pixel(48,48),br=pixel(80,48);
    if(tl[0]<200||tl[1]>30||tr[1]<200||bl[2]<200||br[0]<200||br[1]<200){std::cerr<<"Native scope quadrant orientation/channels failed\n";return 1;}
    auto other_eye=eye+glm::vec3(.064,0,0);draw(other_eye);auto hidden=pixel(64,64);
    if(hidden[0]>100||hidden[1]>100||hidden[2]>100){std::cerr<<"Scope leaked into unaligned eye\n";return 1;}
    source.reset();draw(eye);auto empty=pixel(64,64);
    if(empty[0]>100||empty[1]>100||empty[2]>100){std::cerr<<"Stale scope image retained after reset\n";return 1;}
    GLuint native_scope{};glGenTextures(1,&native_scope);glBindTexture(GL_TEXTURE_2D,native_scope);
    const unsigned char native_pixels[]{0,0,255,255,255,255,0,255,255,0,0,255,0,255,0,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,native_pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    auto draw_native=[&](unsigned texture){
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        auto view=glm::lookAt(eye,glm::vec3(ocular.x,ocular.y,ocular.z),glm::vec3(0,1,0));
        renderer.draw_rifle_texture(projection*view,glm::mat4(1),eye,texture,1,true);
    };
    draw_native(native_scope);tl=pixel(48,80);tr=pixel(80,80);bl=pixel(48,48);br=pixel(80,48);
    if(tl[0]<200||tl[1]>30||tr[1]<200||bl[2]<200||br[0]<200||br[1]<200){std::cerr<<"Native GPU scope orientation failed\n";return 1;}
    for(float scale:{.4f,1.f,1.6f}){
        const glm::vec3 target(ocular.x*scale,ocular.y*scale,ocular.z*scale);
        const glm::vec3 camera=target+glm::vec3(0,0,.1f*scale);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const auto view=glm::lookAt(camera,target,glm::vec3(0,1,0));
        renderer.draw_rifle_texture(projection*view,glm::scale(glm::mat4(1),glm::vec3(scale)),camera,native_scope,1,true);
        const auto quadrant=pixel(48,80);
        if(quadrant[0]<200||quadrant[1]>30){std::cerr<<"Scaled rifle lost scope aperture alignment";return 1;}
    }
    draw_native(0);empty=pixel(64,64);
    if(empty[0]>100||empty[1]>100||empty[2]>100||!glIsTexture(native_scope)){std::cerr<<"Borrowed scope reset/ownership failed\n";return 1;}
    glDeleteTextures(1,&native_scope);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    auto distant=std::make_shared<RasterFrame>(RasterFrame{1,2,1,2,2,1,std::vector<uint32_t>(4,0xff00ff00)});
    renderer.draw_panel(glm::perspective(glm::radians(60.f),1.f,.01f,100.f),distant,10.f);
    auto panel_pixel=pixel(64,64);
    if(panel_pixel[1]<200){std::cerr<<"Valid 10m panel distance produced blank original view\n";return 1;}
    if(glGetError()!=GL_NO_ERROR)return 1;
    glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&target);
    std::cout<<"Production GPU scope: source orientation, color channels, per-eye aperture and reset passed\n";
}
