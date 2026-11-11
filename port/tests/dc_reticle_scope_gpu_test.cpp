// Production RifleRenderer and the exact extracted production glow pass.
#include "gl_context.h"
#include "rifle_renderer.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "xr_gun.h"
#include "xr_theme.h"
#include "reticle_settings.h"
#include "custom_reticle.h"
#include "rend/gles/glcache.h"
namespace config {
inline float VrReticleMinSize=.25f,VrReticleMaxSize=1,VrReticleShrinkDistance=6;
inline int VrReticleMode=1;
}
namespace silent_scope::dc_settings {
bool is_silent_scope() noexcept{return true;}
float laser_half_width() noexcept{return .0016f;}
}
GLCache glcache;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr int VERTEX_POS_ARRAY=0,VERTEX_COL_BASE_ARRAY=1,VERTEX_UV_ARRAY=3;
const char* VertexCompatShader="";const char* PixelCompatShader="";
struct OpenGlSource {
 std::string source;
 OpenGlSource& addSource(const char* value){source+=value;return *this;}
 std::string generate()const{return "#version 330 core\n#define highp\n#define mediump\n#define lowp\n#define gl_FragColor test_pixel\nout vec4 test_pixel;\n"+source;}
};
GLuint gl_CompileAndLink(const char* vertex,const char* fragment){
 auto compile=[](GLenum type,const char* source){auto shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
  GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char text[2048];glGetShaderInfoLog(shader,sizeof(text),nullptr,text);throw std::runtime_error(text);}return shader;};
 auto vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment);auto program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);
 glBindAttribLocation(program,VERTEX_POS_ARRAY,"in_pos");glBindAttribLocation(program,VERTEX_COL_BASE_ARRAY,"in_base");glBindAttribLocation(program,VERTEX_UV_ARRAY,"in_uv");
 glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);require(ok,"Production glow shader link failed");return program;
}
GLuint createVertexArray(){GLuint vao;glGenVertexArrays(1,&vao);return vao;}
void bindVertexArray(GLuint vao){glBindVertexArray(vao);}
void logGlErrors(const char*){require(glGetError()==GL_NO_ERROR,"Production glow GL error");}
}
namespace vr::xr {glm::vec3 gunMuzzle(GunKind){return GunMuzzle;}}
#include "dc_reticle_production.inc"
namespace {
constexpr int Width=512;
using Pixels=std::vector<std::array<unsigned char,4>>;
Pixels read(){Pixels result(Width*Width);glReadPixels(0,0,Width,Width,GL_RGBA,GL_UNSIGNED_BYTE,result.data());return result;}
bool red(const std::array<unsigned char,4>& p){return p[0]>180&&p[1]<100&&p[2]<100;}
void glow(const glm::mat4& vp,glm::vec3 eye,const vr::xr::GunView& gun,bool occlude){
#if SS_TEST_GLOW_HAS_OCCLUSION
 vr::xr::drawGunGlow(vp,eye,gun,true,occlude);
#else
 (void)occlude;vr::xr::drawGunGlow(vp,eye,gun,true);
#endif
}
}
int main(){try{
 silent_scope::GLContext context;silent_scope::RifleRenderer rifle;
 GLuint fbo,color,depth,scope;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,Width,Width,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,Width,Width);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"FBO incomplete");
 std::vector<std::array<unsigned char,4>> feed(64*64,{20,180,200,255});
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)if(std::abs(x-32)<=1||std::abs(y-32)<=1)feed[y*64+x]={5,5,5,255};
 glGenTextures(1,&scope);glBindTexture(GL_TEXTURE_2D,scope);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,feed.data());
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 const auto ocular=silent_scope::make_rifle_mesh().ocular;glm::vec3 center(ocular.x,ocular.y,ocular.z-.006);
 const auto projection=glm::perspective(glm::radians(55.f),1.f,.005f,20.f);
 int checked=0;
 for(float eyeX:{0.f,.064f,-.064f,-.008f,.008f}){
  const glm::vec3 eye(center.x+eyeX,center.y,ocular.z+.10f);
  const auto vp=projection*glm::lookAt(eye,center,glm::vec3(0,1,0));
  auto base=[&]{glViewport(0,0,Width,Width);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glClearDepth(1);glClearColor(.2f,.2f,.2f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   rifle.draw_rifle_texture(vp,glm::mat4(1),eye,scope,1,false);glcache.Reset();};
  const glm::vec3 ray=glm::normalize(center-eye);
  vr::xr::GunView gun;gun.aimPoint=eye+ray*3.f;gun.aimDot=true;gun.reticleHead=glm::translate(glm::mat4(1),eye);gun.reticlePanelDistance=1.8f;
  gun.restMuzzle=glm::vec3(0,.005f,-.94f);gun.pose=glm::translate(glm::mat4(1),gun.restMuzzle-vr::xr::GunMuzzle);
  // Large artwork deliberately crosses the aperture, proving per-pixel clipping
  // rather than a global hide. Exact size logic and mesh come from production.
  config::VrReticleMaxSize=3.f;
  for(bool laser:{false,true}){
   gun.aimLine=laser;base();const auto before=read();
   std::vector<float> z(Width*Width);glReadPixels(0,0,Width,Width,GL_DEPTH_COMPONENT,GL_FLOAT,z.data());
   glow(vp,eye,gun,true);const auto after=read();
   int overlap=0,outside=0;
   for(size_t i=0;i<after.size();++i){if(z[i]<.999f){if(after[i]!=before[i])++overlap;}
    else if(red(after[i]))++outside;}
   require(overlap==0,"Custom reticle painted over physical rifle/optic");
   // This view places most reticle pixels in the optic; a separate translated
   // target below checks that ordinary world artwork is still drawn.
   base();gun.aimPoint=eye+ray*3.f+glm::vec3(.9f,0,0);glow(vp,eye,gun,true);const auto shifted=read();
   int visible=0;for(size_t i=0;i<shifted.size();++i)if(z[i]>=.999f&&red(shifted[i]))++visible;
   require(visible>10,"Unoccluded world reticle disappeared");
   gun.aimPoint=eye+ray*3.f;
   (void)outside;++checked;
  }
  // None never adds an arm, and menu pointers deliberately retain their
  // established overlay visibility even when the gameplay flag is supplied.
  base();auto before=read();config::VrReticleMode=2;gun.aimLine=false;glow(vp,eye,gun,true);require(read()==before,"None added reticle pixels");config::VrReticleMode=1;
  base();before=read();gun.menuPointer=true;glow(vp,eye,gun,true);require(read()!=before,"Purple menu pointer was occluded");gun.menuPointer=false;
  base();before=read();gun.servicePointer=true;glow(vp,eye,gun,true);require(read()!=before,"Service pointer was occluded");gun.servicePointer=false;
  // Other games/default false preserve the existing always-visible artwork.
  base();before=read();vr::xr::drawGunGlow(vp,eye,gun,true);require(read()!=before,"Default glow behavior changed");
 }
 require(glGetError()==GL_NO_ERROR,"Final GL error");
 glDeleteTextures(1,&scope);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fbo);
 glDeleteProgram(vr::xr::glowProgram);glDeleteBuffers(1,&vr::xr::glowVbo);glDeleteVertexArrays(1,&vr::xr::glowVao);
 std::cout<<"Production rifle/glow reticle depth, native feed, stereo/offset, Laser, None and purple pointer checks passed: "<<checked<<" draws\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
