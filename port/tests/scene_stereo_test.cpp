#include "scene/gl_scene.h"
#include "scene/scene_geometry.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
using namespace silent_scope::scene;
int main(){
    silent_scope::GLContext context;GLScene renderer;
    GLuint target{},color{},depth{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,384,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,384);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 1;
    auto frame=std::make_shared<RawFrame>();frame->width=512;frame->height=384;frame->lens={512,384,443,443,256,192};
    frame->complete=true;frame->sequence=1;frame->clear_color=0xff000000;
    for(int i=0;i<2;++i){RawTriangle triangle;triangle.state.depth_mode=512;
        // Native Silent Scope gameplay vertices have depths in the thousands.
        double z=(i?4:2)/SceneMetresPerUnit,center=(i?.6:-.4)/SceneMetresPerUnit;
        const double shape[3][2]={{-.15,-.15},{.15,-.15},{0,.15}};
        for(int v=0;v<3;++v){auto& p=triangle.vertex[v];p.x=256+443*(center+shape[v][0]/SceneMetresPerUnit)/z;p.y=192-443*shape[v][1]/SceneMetresPerUnit/z;p.q=1/z;p.a=255;p.r=i?0:255;p.g=i?255:0;p.b=0;}
        frame->triangles.push_back(triangle);
    }
    renderer.prepare(frame,false);
    auto capture=[&](float eye,bool red_may_be_missing=false){
        glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,512,384);
        auto projection=glm::frustum(-256.f/443*.01f,256.f/443*.01f,-192.f/443*.01f,192.f/443*.01f,.01f,100.f);
        renderer.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),SceneMetresPerUnit,1.8);
        std::vector<unsigned char> pixels(512*384*4);glReadPixels(0,0,512,384,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        double x[2]{},count[2]{};for(int y=0;y<384;++y)for(int column=0;column<512;++column){auto n=(y*512+column)*4;int i=pixels[n]>200&&pixels[n+1]<10?0:pixels[n+1]>200&&pixels[n]<10?1:-1;if(i>=0){x[i]+=column;++count[i];}}
        if((!count[0]&&!red_may_be_missing)||!count[1])throw std::runtime_error("Native geometry missing from GPU output");
        return std::array<double,2>{count[0]?x[0]/count[0]:NAN,x[1]/count[1]};
    };
    auto left=capture(-.032f),right=capture(.032f);auto near=left[0]-right[0],far=left[1]-right[1];
    GLint sampler{};glGetIntegeri_v(GL_SAMPLER_BINDING,0,&sampler);
    if(sampler){std::cerr<<"World sampler leaked into rifle scope rendering\n";return 1;}
    if(near<=far*1.7||far<=2||near<10){std::cerr<<"Missing depth-dependent stereo disparity "<<near<<' '<<far<<'\n';return 1;}
    // Two otherwise identical consecutive primitives require different native
    // raster clips, including after moving the eye. They must not batch together.
    auto clipped=std::make_shared<RawFrame>(*frame);clipped->sequence=2;
    for(auto& triangle:clipped->triangles){triangle.clip_left=0;triangle.clip_right=512;triangle.clip_top=0;triangle.clip_bottom=384;}
    clipped->triangles[0].clip_left=256;
    renderer.prepare(clipped,false);
    auto restricted=capture(-.032f,true);
    if(!std::isnan(restricted[0])||!std::isfinite(restricted[1])){std::cerr<<"Native raster clip ignored\n";return 1;}
    // HUD resizing must work for genuine stereo geometry even without the
    // unrelated reconstructed-depth-background feature.
    auto hud=std::make_shared<RawFrame>();hud->width=512;hud->height=384;hud->lens=frame->lens;hud->complete=true;hud->sequence=3;
    const double corners[4][2]={{200,150},{312,150},{200,234},{312,234}};
    for(auto ids:{std::array<int,3>{0,1,2},std::array<int,3>{2,1,3}}){RawTriangle t;t.overlay=true;t.state.depth_mode=512;
        for(int k=0;k<3;++k){auto& v=t.vertex[k];v.x=corners[ids[k]][0];v.y=corners[ids[k]][1];v.q=1/1.1;v.r=255;v.a=255;}
        hud->triangles.push_back(t);}
    renderer.prepare(hud,false);
    auto hud_area=[&](float size){auto p=glm::frustum(-256.f/443*.01f,256.f/443*.01f,-192.f/443*.01f,192.f/443*.01f,.01f,100.f);
        renderer.draw(p,SceneMetresPerUnit,1.8,1,{},size);std::vector<unsigned char> pixels(512*384*4);glReadPixels(0,0,512,384,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        size_t count=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]>200&&pixels[i+1]<10)++count;return count;};
    const auto normal_hud=hud_area(1),large_hud=hud_area(1.5f);
    if(!normal_hud||large_hud<normal_hud*2){std::cerr<<"HUD size ignored by stereo presentation\n";return 1;}
    if(glGetError()!=GL_NO_ERROR)return 1;
    std::cout<<"Real native triangles have independent-eye disparity: near "<<near<<"px, far "<<far<<"px\n";
    glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&target);
}
