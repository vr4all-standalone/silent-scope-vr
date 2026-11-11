// SPDX-License-Identifier: GPL-2.0-or-later
// Desktop GL boundary test. Shader + new gldraw helpers are verbatim production
// extracts, not replacement GLSL. Narrow host-state stubs stand in for Flycast;
// all shader compilation, uniforms, indexed draws and feedback/readback are real.
#include "gl_context.h"
#include <glad/gl.h>
#include "dreamcast/dc_hud.h"
#include "dreamcast/dc_scope_hud.h"
#include "dreamcast/dc_scope_gl.h"
#include "dreamcast/dc_native_primer.h"
#include "fixtures/dc_hud_native_shader.h"
#include "fixtures/dc_native_primer_shader_hooks.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using u32=uint32_t;
constexpr u32 ListType_Translucent=2;
constexpr u32 ListType_Opaque=0,ListType_Punch_Through=4;
static void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct Vertex {float x{},y{},z{},u{},v{};uint8_t col[4]{255,255,255,255};};
struct PolyParam {union Word{u32 full;struct{u32 low:27;u32 CullMode:2;u32 high:3;};};Word isp{},tsp{},tcw{},pcw{};u32 tileclip{},first{},count{};};
struct Program {GLuint program{};GLint vrOverlay{-1},vrHudSize{-1};};
struct Context {
    unsigned framebufferWidth{640},framebufferHeight{480};bool isRTT{};
    struct {bool supported{true};uint64_t generation{3},submit_sequence{11};}dcCamera;
    struct {bool supported{},valid{};float factor{},width{},height{},fx{},fy{},cx{},cy{};uint64_t generation{},submit_sequence{};}dcLens;
    std::vector<Vertex> verts;std::vector<u32> idx;
    struct Sorted {u32 first{},count{},polyIndex{};};std::vector<Sorted> sortedTriangles;
    struct Pass {bool autosort{};u32 tr_count{},sorted_tr_count{};};std::vector<Pass> render_passes;
    std::vector<PolyParam> global_param_op,global_param_pt,global_param_tr;
};
static struct {Context* rendContext{};GLenum index_type{GL_UNSIGNED_INT};Program modvol_shader;size_t get_index_size()const{return 4;}}gl;
static struct {struct {std::string gameId{"T9507N"};}content;}settings;
namespace config {static bool VrReproject=true;static float VrHudSize=1.f;}
static float metres_per_unit=.1f,panel_metres=1.8f;
static bool xr_eye=true;
static bool simulation_empty=false;
namespace vr {float worldScale(){return metres_per_unit;} namespace xr {bool simulationMagazineEmpty(){return simulation_empty;} const void* currentEye(){return xr_eye?&xr_eye:nullptr;}}}
namespace silent_scope::dc_settings {
float panel_distance_metres()noexcept{return panel_metres;}
}
static Program* CurrentShader{};
static struct {glm::vec4 vrOverlay{silent_scope::dc_hud::NativeTanX,silent_scope::dc_hud::NativeTanY,18.f,1.f};float vrHudSize=1.f;}ShaderUniforms;
// Production SetGPState selects a cached program and updates per-polygon size.
// Deliberately do NOT reupload overlay here: the actual hook must restore it.
static void SetCull(u32 cull){if(cull<2)glDisable(GL_CULL_FACE);else {glEnable(GL_CULL_FACE);glCullFace(cull==2?GL_FRONT:GL_BACK);}}
template<u32 Type,bool Sorted> static void SetGPState(const PolyParam* p,u32 flip=0){
    // Flycast glcache skips a same-program bind. Required during transform
    // feedback too: glUseProgram itself is illegal while capture is active.
    GLint active{};glGetIntegerv(GL_CURRENT_PROGRAM,&active);
    if(GLuint(active)!=CurrentShader->program)glUseProgram(CurrentShader->program);
    if(CurrentShader->vrHudSize!=-1)glUniform1f(CurrentShader->vrHudSize,ShaderUniforms.vrHudSize);
    const u32 cull=((p->isp.full>>27)&3)^flip^1;
    SetCull(cull);
}
#include "fixtures/dc_native_primer_hook.inc"
#include "fixtures/dc_hud_native_hook.inc"
#include "fixtures/dc_hud_native_depth_hook.inc"
#include "fixtures/dc_native_primer_depth_hook.inc"
// Keep test conveniences after the verbatim helpers: an alias here must not
// hide a missing namespace in their real production translation unit.
namespace hud=silent_scope::dc_hud;

static GLuint compile(GLenum type,const std::string& text){
    const GLuint shader=glCreateShader(type);const char* ptr=text.c_str();glShaderSource(shader,1,&ptr,nullptr);glCompileShader(shader);
    GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof log,nullptr,log);throw std::runtime_error(log);}return shader;
}
static Program program(const std::string& source,bool vr,bool color){
    const std::string prefix="#version 330 core\n#define TARGET_GL 3\n#define GLES2 2\n#define DIV_POS_Z 1\n#define INTERPOLATION\n#define VR_REPROJECT "+std::to_string(vr?1:0)+"\n#define pp_Gouraud "+std::to_string(color?1:0)+"\n";
    const GLuint vs=compile(GL_VERTEX_SHADER,prefix+source);
    const GLuint fs=compile(GL_FRAGMENT_SHADER,color?"#version 330 core\nin vec4 vtx_base;out vec4 color;void main(){color=vtx_base;}":"#version 330 core\nvoid main(){}");
    Program p;p.program=glCreateProgram();glAttachShader(p.program,vs);glAttachShader(p.program,fs);
    glBindAttribLocation(p.program,0,"in_pos");glBindAttribLocation(p.program,1,"in_base");
    glBindAttribLocation(p.program,2,"in_offs");glBindAttribLocation(p.program,3,"in_uv");
    const char* varying="gl_Position";glTransformFeedbackVaryings(p.program,1,&varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p.program);
    GLint ok{};glGetProgramiv(p.program,GL_LINK_STATUS,&ok);if(!ok){char log[4096]{};glGetProgramInfoLog(p.program,sizeof log,nullptr,log);throw std::runtime_error(log);}
    glDeleteShader(vs);glDeleteShader(fs);p.vrOverlay=glGetUniformLocation(p.program,"vrOverlay");p.vrHudSize=glGetUniformLocation(p.program,"vrHudSize");return p;
}
static std::string load_shader(int argc,char** argv){
    if(argc<2)return hud_gpu_fixture::VertexSource;
    std::ifstream file(argv[1],std::ios::binary);require(bool(file),"Cannot open actual gles.cpp shader source");
    std::string text((std::istreambuf_iterator<char>(file)),{});require(text.size()<2*1024*1024,"Unbounded shader input");
    std::string lf;for(char c:text)if(c!='\r')lf+=c;
    const std::string marker="static const char* VertexShaderSource = R\"(";const auto begin=lf.find(marker);
    require(begin!=std::string::npos&&lf.find(marker,begin+1)==std::string::npos,"Missing/duplicate native vertex shader");
    const auto end=lf.find(")\";",begin+marker.size());require(end!=std::string::npos,"Truncated native shader");
    auto source=lf.substr(begin+marker.size(),end-begin-marker.size());
    require(source==hud_gpu_fixture::VertexSource,"Native vertex shader changed; regenerate and review fixture");
    if(argc>2){
        std::ifstream hook_file(argv[2],std::ios::binary);require(bool(hook_file),"Cannot open actual gldraw.cpp");
        std::string hook_text((std::istreambuf_iterator<char>(hook_file)),{}),normalized;
        require(hook_text.size()<1024*1024,"Unbounded renderer source");for(char c:hook_text)if(c!='\r')normalized+=c;
        const std::string start="#if !defined(GLES2)\nstatic silent_scope::dc_hud::Gate silentScopeHudGate";
        const auto at=normalized.find(start),stop=normalized.find("template <u32 Type, bool SortingEnabled>\nvoid DrawList",at);
        require(at!=std::string::npos&&stop!=std::string::npos,"Missing actual native HUD hook");
        require(normalized.substr(at,stop-at)==hud_gpu_fixture::HookSource,"Native HUD hook changed; regenerate and review fixture");
        const auto depth_start=normalized.find("#if !defined(GLES2)\n                // The depth-only pass must follow");
        const auto depth_stop=normalized.find("\n\n\t\t\t\tglDrawElements(GL_TRIANGLES",depth_start);
        require(depth_start!=std::string::npos&&depth_stop!=std::string::npos,"Missing actual native HUD depth hook");
        require(normalized.substr(depth_start,depth_stop-depth_start)==hud_gpu_fixture::DepthHookSource,"Native HUD depth hook changed; regenerate and review fixture");
        require(normalized.find(primer_gpu_fixture::Helper)!=std::string::npos&&normalized.find(primer_gpu_fixture::Depth)!=std::string::npos,
            "Native list-primer renderer/depth hooks changed; regenerate and review fixture");
    }
    return source;
}
struct Gpu {
    GLuint vao{},vbo{},ebo{},feedback{},query{},fbo{},rgba{},depth{};
    Gpu(){
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glGenBuffers(1,&ebo);glGenBuffers(1,&feedback);glGenQueries(1,&query);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&rgba);glBindTexture(GL_TEXTURE_2D,rgba);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,rgba,0);
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,256,256,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"GPU test framebuffer incomplete");glViewport(0,0,256,256);
    }
    ~Gpu(){glDeleteQueries(1,&query);glDeleteBuffers(1,&feedback);glDeleteBuffers(1,&ebo);glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&depth);glDeleteTextures(1,&rgba);glDeleteFramebuffers(1,&fbo);}
    void bind(Context& ctx,Program& p){
        gl.rendContext=&ctx;CurrentShader=&p;glUseProgram(p.program);glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(ctx.verts.size()*sizeof(Vertex)),ctx.verts.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(ctx.idx.size()*4),ctx.idx.data(),GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,col)));
        glDisableVertexAttribArray(2);glVertexAttrib4f(2,0,0,0,0);glEnableVertexAttribArray(3);glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,u)));
        glm::mat4 ndc(1);ndc[0][0]=1.f/320;ndc[1][1]=1.f/240;ndc[3][0]=-1;ndc[3][1]=-1;
        glUniformMatrix4fv(glGetUniformLocation(p.program,"ndcMat"),1,GL_FALSE,&ndc[0][0]);
        const auto camera=glm::perspective(glm::radians(90.f),1.f,.05f,100.f)*glm::scale(glm::mat4(1),glm::vec3(metres_per_unit));
        glUniformMatrix4fv(glGetUniformLocation(p.program,"vrMat"),1,GL_FALSE,&camera[0][0]);
        glUniform2f(glGetUniformLocation(p.program,"vrTan"),hud::NativeTanX,hud::NativeTanY);
        glUniform3f(glGetUniformLocation(p.program,"vrComfort"),0,0,0);glUniform1f(glGetUniformLocation(p.program,"vrWorldZoom"),1);
        if(p.vrOverlay!=-1)glUniform4fv(p.vrOverlay,1,&ShaderUniforms.vrOverlay[0]);
        if(p.vrHudSize!=-1)glUniform1f(p.vrHudSize,ShaderUniforms.vrHudSize);
        glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glEnable(GL_PRIMITIVE_RESTART);glPrimitiveRestartIndex(UINT32_MAX);
    }
    std::vector<glm::vec4> draw(Context& ctx,const PolyParam& pp,Program& p,bool triangles,bool adapter=true,bool depth_only=false){
        bind(ctx,p);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(depth_only){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);}else glDisable(GL_DEPTH_TEST);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,65536,nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,query);glBeginTransformFeedback(GL_TRIANGLES);
        bool handled=false;
#ifndef SS_HUD_TEST_DISABLE_ADAPTER
        if(adapter&&depth_only){gl.modvol_shader=p;ctx.sortedTriangles={{0,u32(ctx.idx.size())}};drawExtractedNativePrimerDepth(&pp);handled=true;}
        else if(adapter){
            handled=drawSilentScopeNativePrimers<ListType_Translucent,true>(&pp,0,u32(ctx.idx.size()),triangles,0);
            if(!handled)handled=drawSilentScopeHud<ListType_Translucent,true>(&pp,0,u32(ctx.idx.size()),triangles);
        }
#else
        (void)adapter;(void)pp;
#endif
        if(!handled)glDrawElements(triangles?GL_TRIANGLES:GL_TRIANGLE_STRIP,GLsizei(ctx.idx.size()),GL_UNSIGNED_INT,nullptr);
        glEndTransformFeedback();glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        GLuint count{};glGetQueryObjectuiv(query,GL_QUERY_RESULT,&count);require(count<100,"Unexpected feedback primitive count");
        std::vector<glm::vec4> out(count*3);glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,GLsizeiptr(out.size()*sizeof(glm::vec4)),out.data());
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);require(glGetError()==GL_NO_ERROR,"OpenGL test error");return out;
    }
    std::vector<uint8_t> pixels(){std::vector<uint8_t> out(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,out.data());return out;}
    std::vector<glm::vec4> scope_widget(Context& ctx,Program& p,uintptr_t context,bool rifle=false,bool adapt=true,bool depth_only=false){
        bind(ctx,p);gl.modvol_shader=p;glBindFramebuffer(GL_FRAMEBUFFER,fbo);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
        glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(depth_only){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);}else glDisable(GL_DEPTH_TEST);
        silent_scope::dc_scope::ScopedRifleDrawAuthorization authorization(rifle);
        silent_scope::dc_scope::ScopedCompositeDraw composite(settings.content.gameId,ctx,config::VrReproject&&xr_eye,context);
        bool fallback_enabled=adapt&&config::VrReproject&&xr_eye;
#ifdef SS_SCOPE_FALLBACK_DISABLED
        fallback_enabled=false; // Negative control: expose the original near-face widget bug.
#endif
        silent_scope::dc_scope_hud::ScopedDraw fallback(settings.content.gameId,ctx,fallback_enabled);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,65536,nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
        glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,query);glBeginTransformFeedback(GL_TRIANGLES);
        for(unsigned i=0;i<ctx.sortedTriangles.size();++i){const auto& draw=ctx.sortedTriangles[i];const auto& polygon=ctx.global_param_tr[draw.polyIndex];
            if(silent_scope::dc_scope::should_skip_scope_composite(ctx,draw.polyIndex))continue;
            if(depth_only){drawExtractedNativeDepth(&polygon,int(i));continue;}
            bool handled=false;if(adapt)handled=drawSilentScopeHud<ListType_Translucent,true>(&polygon,draw.first,draw.count,true,draw.polyIndex);
            if(!handled)glDrawElements(GL_TRIANGLES,draw.count,GL_UNSIGNED_INT,reinterpret_cast<void*>(size_t(draw.first)*4));
        }
        glEndTransformFeedback();glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);GLuint count{};glGetQueryObjectuiv(query,GL_QUERY_RESULT,&count);
        require(count<=92,"Unexpected native scope primitive count");std::vector<glm::vec4> out(count*3);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,GLsizeiptr(out.size()*sizeof(glm::vec4)),out.data());glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        require(glGetError()==GL_NO_ERROR,"Native scope GL error");return out;
    }
};
static void close(float a,float b,const char* why){require(std::isfinite(a)&&std::abs(a-b)<2e-5f,why);}
static PolyParam font(){return {{0xc8800000},{0x9491a4ed},{0x500c0000},{0x828c000a},0x100200};}
static Context mixed(){Context c;c.verts={{220,30,.7299270034f},{300,30,.7299270034f},{220,50,.7299270034f},
    {220,200,.7299270034f},{300,200,.7299270034f},{220,220,.7299270034f}};c.idx={0,1,2,3,4,5};return c;}
static Context targets(){Context c;c.verts={
    {100,200,.333333343267f,16.5f/128,1.5f/128,{176,176,176,255}},
    {105.6376953125f,209.39616394043f,.333333343267f,17.f/128,0,{176,176,176,255}},
    {111.275390625f,200,.333333343267f,18.f/128,1.5f/128,{176,176,176,255}},
    {100,100,.333333343267f,.5f,0,{176,176,176,255}},
    {137.584656f,100,.333333343267f,1,0,{176,176,176,255}},
    {137.584656f,115.033867f,.333333343267f,1,15.5f/128,{176,176,176,255}},
    {200,200,.333333343267f,.5f,0,{176,176,176,255}},
    {230,200,.333333343267f,1,0,{176,176,176,255}},
    {200,230,.333333343267f,1,15.5f/128,{176,176,176,255}}};c.idx={0,1,2,3,4,5,6,7,8};return c;}
static Context hurry_up(){Context c;
    const auto add=[&](float x,float y,float w,float h,bool plate,unsigned cell=0){PolyParam p;
        p.first=u32(c.verts.size());p.count=4;p.isp.full=0xc8800000;p.tsp.full=plate?0x9491a4e4:0x9491a4ed;
        p.tcw.full=plate?0x3f308d56:0x500c0000;p.pcw.full=plate?0x828c0002:0x828c000a;p.tileclip=0x100200;
        const float u=(cell&15)/16.f,v=(cell>>4)/16.f,q=plate?.7246376872062683f:.7299270033836365f;
        const uint8_t color=plate?0:255,alpha=plate?112:255;
        for(unsigned j=0;j<4;++j)c.verts.push_back({x+(j&2?w:0),y+(j&1?h:0),q,u+(j&2?1.f/16:0),v+(j&1?1.f/16:0),{color,color,color,alpha}});
        const u32 first=u32(c.idx.size()),poly=u32(c.global_param_tr.size());c.idx.insert(c.idx.end(),{p.first,p.first+1,p.first+2,p.first+2,p.first+1,p.first+3});
        c.sortedTriangles.push_back({first,6,poly});c.global_param_tr.push_back(p);
    };
    add(80,390,440,90,true);const char phrase[]="Hurry up!";for(unsigned i=0;i<9;++i)add(90+10.f*i,390,10,20,false,unsigned(phrase[i])-32);
    c.render_passes={{true,10,10}};return c;
}
static Context scope_geometry(float native_scale=1){
    Context c;const auto append=[&](const std::vector<Vertex>& vertices,bool rim){PolyParam p;
        p.first=u32(c.verts.size());p.count=u32(vertices.size());p.isp.full=0xc8800000;p.tsp.full=rim?0x9491a4db:0x9491a4ed;
        p.tcw.full=rim?0x5004c800:0x0c0a5800;p.pcw.full=0x828c002a;p.tileclip=0x100200;
        const u32 id=u32(c.global_param_tr.size()),first=u32(c.idx.size());c.verts.insert(c.verts.end(),vertices.begin(),vertices.end());
        for(u32 i=2;i<p.count;++i)c.idx.insert(c.idx.end(),{p.first+i-2,p.first+i-1,p.first+i});
        c.sortedTriangles.push_back({first,u32(c.idx.size())-first,id});c.global_param_tr.push_back(p);
    };
    const auto point=[](int i){const double angle=i*6.2831853071795864769/64;
        return Vertex{float(320+120.5890197753906*std::cos(angle)),float(240+112.6999816894531*std::sin(angle)),.625f,
            float(std::clamp(.5+.5*std::cos(angle),1./256,255./256)),float(std::clamp(127./256+.5*std::sin(angle),0.,254./256)),{254,254,254,255}};};
    const Vertex center{320,240,.625f,.5f,127.f/256,{254,254,254,255}};append({point(0),point(1),center},false);
    for(int i=0;i<21;++i){const int a=1+i*3;append({point(a),point(a+1),center,point(a+2),point(a+3)},false);}
    const int counts[]{8,9,9,10};int quadrant=0;
    for(int sx:{-1,1})for(int sy:{-1,1}){std::vector<Vertex> rim;
        const float xy[][2]{{123.05f,0},{116,6},{123.05f,57.5f},{80,74},{61.525f,115},{7.21f,107.18f},{0,115},{-.17f,107.18f},{7.21f,107.18f},{61.525f,115}};
        for(int i=0;i<counts[quadrant];++i)rim.push_back({320+sx*xy[i][0],240+sy*xy[i][1],.6289308071136475f,.5f,.5f,{176,176,176,255}});
        ++quadrant;append(rim,true);
    }
    for(auto& v:c.verts){v.x=320+(v.x-320)/native_scale;v.y=240+(v.y-240)/native_scale;}
    c.render_passes={{true,26,u32(c.sortedTriangles.size())}};return c;
}
int main(int argc,char** argv){try{
    const auto source=load_shader(argc,argv);silent_scope::GLContext context;Gpu gpu;
    std::cout<<"Desktop GL "<<glGetString(GL_RENDERER)<<" / "<<glGetString(GL_VERSION)<<'\n';
    auto color=program(source,true,true),depth=program(source,true,false),original=program(source,false,true);
    auto ctx=mixed();const auto pp=font();auto positions=gpu.draw(ctx,pp,color,true);
    require(positions.size()==6,"Mixed draw lost triangles");for(unsigned i=0;i<3;++i)close(positions[i].w,1.8f,"Measured HUD was not placed at1.8m");
    for(unsigned i=3;i<6;++i)close(positions[i].w,.137000017f,"HUD uniform leaked to equal-q world geometry");
    panel_metres=2.5f;auto farther=gpu.draw(ctx,pp,color,true);for(unsigned i=0;i<3;++i)close(farther[i].w,2.5f,"Saved panel distance ignored by production shader");
    auto depth_positions=gpu.draw(ctx,pp,depth,true,true,true);require(depth_positions.size()==farther.size(),"Depth-only pass lost geometry");
    for(unsigned i=0;i<farther.size();++i)for(unsigned j=0;j<4;++j)close(depth_positions[i][j],farther[i][j],"Depth/color shader placement disagrees");
    float z{};glReadPixels(111,81,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);const float expected_depth=.5f*((100.f+.05f)/(100.f-.05f)-2*100*.05f/((100.f-.05f)*2.5f))+.5f;
    close(z,expected_depth,"Depth-only HUD raster not at saved physical plane");
    auto black=gpu.pixels();for(auto byte:black)require(byte==0,"Depth-only shader changed color attachment");
    panel_metres=1.8f;Context ammo;PolyParam ammo_p{{0xc8800000},{0x9491a4e4},{0x500c7900},{0x828c002a},0x100200};
    ammo.verts={{10,390,.6451612115f,0,0,{176,176,176,255}},{30,390,.6451612115f,0,0,{176,176,176,255}},{10,410,.6451612115f,0,0,{176,176,176,255}}};ammo.idx={0,1,2};
    auto stock=gpu.draw(ammo,ammo_p,color,true);
    require(stock.size()==3,"Arcade ammo was not drawn");
    simulation_empty=true;
    require(gpu.draw(ammo,ammo_p,color,true).empty(),"Native ammo was drawn in Simulation");
    require(gpu.draw(ammo,ammo_p,depth,true,true,true).empty(),"Hidden native ammo still wrote depth");
    simulation_empty=false;
    require(gpu.draw(ammo,ammo_p,color,true).size()==3,"Ammo did not return after seating a magazine");
    for(auto& v:ammo.verts){v.x=320+(v.x-320)/3;v.y=240+(v.y-240)/3;}
    auto shrunk=gpu.draw(ammo,ammo_p,color,true);for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)close(shrunk[i][j],stock[i][j],"Native /3 UI framing was not restored");
    config::VrHudSize=.5f;ShaderUniforms.vrHudSize=.5f;
    auto small_ignored=gpu.draw(ammo,ammo_p,color,true);for(unsigned i=0;i<3;++i)close(small_ignored[i].x,stock[i].x,"HUD size applied without actual Wider acknowledgment");
    ammo.dcLens={true,true,3,640,480,185,185,320,240,3,11};auto small=gpu.draw(ammo,ammo_p,color,true);
    for(unsigned i=0;i<3;++i){close(small[i].x,stock[i].x*.5f,"Native undo and user size composed incorrectly");close(small[i].w,1.8f,"HUD size changed physical depth");}
    ammo.dcLens.submit_sequence=12;auto stale=gpu.draw(ammo,ammo_p,color,true);
    for(unsigned i=0;i<3;++i)close(stale[i].x,stock[i].x,"Stale submitted lens enabled HUD sizing");
    auto indicators=targets();PolyParam target_p{{0xc8800000},{0x9491a4e4},{0x100b0a00},{0x828c002a},0x100200};
    config::VrHudSize=1;ShaderUniforms.vrHudSize=1;
    // Exact native list primers are transparent native offscreen sprites. They
    // must contribute neither VR triangles/depth nor CPU targets after unprojection.
    Context primer;PolyParam primer_p{{0x09000000},{0x00800440},{0},{0xa0800000},0x100200,0,4};
    primer.verts={{-1.5f,-.5f,.1f,0,0,{0,0,0,0}},{-.5f,-.5f,.1f,0,0,{0,0,0,0}},
        {-1.5f,-1,.1f,0,0,{0,0,0,0}},{-.5f,-1,.1f,0,0,{0,0,0,0}}};primer.idx={0,1,2,1,2,3};
    primer.global_param_tr={primer_p};primer.render_passes={{true,1,1}};primer.sortedTriangles={{0,6,0}};
    require(gpu.draw(primer,primer_p,color,true).empty(),"VR color path drew a native list primer");
    require(gpu.draw(primer,primer_p,depth,true,true,true).empty(),"VR depth path drew a native list primer");
    primer.verts[3].col[3]=1;
    require(gpu.draw(primer,primer_p,color,true).size()==6,"Damaged primer quad was guessed away");
    require(gpu.draw(primer,primer_p,depth,true,true,true).size()==6,"Damaged primer depth was guessed away");
    primer.verts[3].col[3]=0;
    auto target_stock=gpu.draw(indicators,target_p,color,true);require(target_stock.size()==9,"Target draw lost primitives");
    for(unsigned i=0;i<6;++i)close(target_stock[i].w,1.8f,"Target indicator remained at30cm");
    for(unsigned i=6;i<9;++i)close(target_stock[i].w,.3f,"Same-atlas unproven world gained target exception");
    for(unsigned i=0;i<6;++i){auto& v=indicators.verts[i];v.x=320+(v.x-320)/3;v.y=240+(v.y-240)/3;}
    indicators.dcLens={true,true,3,640,480,185,185,320,240,3,11};config::VrHudSize=.5f;ShaderUniforms.vrHudSize=.5f;
    auto target_wide=gpu.draw(indicators,target_p,color,true);
    for(unsigned i=0;i<6;++i)for(unsigned j=0;j<4;++j)close(target_wide[i][j],target_stock[i][j],"Target bearing moved under native /3 or HUD size");
    panel_metres=2.5f;auto target_color=gpu.draw(indicators,target_p,color,true);auto target_depth=gpu.draw(indicators,target_p,depth,true,true,true);
    for(unsigned i=0;i<9;++i)for(unsigned j=0;j<4;++j)close(target_color[i][j],target_depth[i][j],"Target depth/color placement differs");
    for(unsigned i=0;i<6;++i)close(target_depth[i].w,2.5f,"Target ignored saved physical distance");
    panel_metres=1.8f;
    config::VrHudSize=1;ShaderUniforms.vrHudSize=1;
    // A strip changing classification on an odd triangle, followed by restart.
    auto strip=mixed();strip.verts.resize(4);strip.verts[3]={300,200,.7299270034f};strip.idx={0,1,2,3,UINT32_MAX,0,1,2};
    auto split=gpu.draw(strip,pp,color,false);require(split.size()==9,"Strip split/restart lost real GPU primitives");
    for(unsigned i=0;i<3;++i)close(split[i].w,1.8f,"First strip HUD primitive missing");
    for(unsigned i=3;i<6;++i)close(split[i].w,.137000017f,"Odd split inherited HUD depth");
    for(unsigned i=6;i<9;++i)close(split[i].w,1.8f,"Restart failed to restore HUD classification");
    const auto identity=reinterpret_cast<uintptr_t>(context.context());
    auto advisory=hurry_up();auto message_stock=gpu.scope_widget(advisory,color,identity);require(message_stock.size()==60,"Advisory lost native group triangles");
    for(const auto& v:message_stock)close(v.w,1.8f,"Native advisory remained at14cm");
    advisory.dcLens={true,true,3,640,480,185,185,320,240,3,11};config::VrHudSize=.5f;ShaderUniforms.vrHudSize=.5f;panel_metres=2.5f;
    auto message_small=gpu.scope_widget(advisory,color,identity);auto message_depth=gpu.scope_widget(advisory,depth,identity,false,true,true);
    for(unsigned i=0;i<60;++i){close(message_small[i].w,2.5f,"Advisory ignored saved panel metres");
        close(message_small[i].x/message_small[i].w,message_stock[i].x/message_stock[i].w*.5f,"Advisory ignored actual-Wider saved HUD size");
        for(unsigned j=0;j<4;++j)close(message_small[i][j],message_depth[i][j],"Advisory color/depth placement mismatch");}
    auto incomplete=advisory;incomplete.verts.back().col[3]=254;auto retained_message=gpu.scope_widget(incomplete,color,identity);
    for(const auto& v:retained_message)require(v.w<.2f,"Partial advisory group was guessed into HUD");
    config::VrHudSize=1;ShaderUniforms.vrHudSize=1;panel_metres=1.8f;
    auto scope=scope_geometry();auto scope_plane=gpu.scope_widget(scope,color,identity);
    require(scope_plane.size()==92*3,"Native scope fallback lost geometry");for(const auto& v:scope_plane)close(v.w,1.8f,"Unmasked scope widget remained near the face");
    auto scope3=scope_geometry(3);auto restored_scope=gpu.scope_widget(scope3,color,identity);
    require(restored_scope.size()==scope_plane.size(),"Scale3 native scope lost geometry");
    for(unsigned i=0;i<scope_plane.size();++i)for(unsigned j=0;j<4;++j)close(restored_scope[i][j],scope_plane[i][j],"Scope fallback native /3 framing not restored");
    config::VrHudSize=.5f;ShaderUniforms.vrHudSize=.5f;
    auto scope_size_ignored=gpu.scope_widget(scope3,color,identity);
    for(unsigned i=0;i<scope_plane.size();++i)close(scope_size_ignored[i].x,scope_plane[i].x,"Scope HUD size applied without actual Wider");
    scope3.dcLens={true,true,3,640,480,185,185,320,240,3,11};auto scope_small=gpu.scope_widget(scope3,color,identity);
    for(unsigned i=0;i<scope_plane.size();++i){close(scope_small[i].x,scope_plane[i].x*.5f,"Scope native undo/user size composition wrong");close(scope_small[i].w,1.8f,"Scope HUD size changed depth");}
    config::VrHudSize=1;ShaderUniforms.vrHudSize=1;
    auto damaged_scope=scope;damaged_scope.verts[0].z=.624f;auto preserved=gpu.scope_widget(damaged_scope,color,identity);
    require(preserved.size()==92*3,"Unknown scope group was removed");for(const auto& v:preserved)require(v.w<.2f,"Damaged scope group was guessed into HUD");
    panel_metres=2.5f;auto scope_depth=gpu.scope_widget(scope3,depth,identity,false,true,true);
    for(const auto& v:scope_depth)close(v.w,2.5f,"Depth-only scope fallback ignored saved panel metres");
    glReadPixels(128,128,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);close(z,expected_depth,"Scope fallback depth raster not at saved plane");
    // Use an actual owned GPU scope copy, then the same real authorization and
    // skip predicate as production. Native widget must not render a second eye.
    GLuint source_fbo{},source_texture{};glGenFramebuffers(1,&source_fbo);glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);
    glGenTextures(1,&source_texture);glBindTexture(GL_TEXTURE_2D,source_texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB565,640,480,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,source_texture,0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Scope-copy source incomplete");
    auto& owned=silent_scope::dc_scope::gpu_scope();require(owned.initialize(silent_scope::dc_scope::current_gl_api(),identity),"Cannot initialize real scope GPU owner");owned.set_physical_rifle_active(true);
    silent_scope::dc_scope::Source source_scope{"T9507N",true,true,640,480,0x52c000,1,512,0,0,256,256,10,3,source_fbo};
    require(owned.capture(source_scope,identity,silent_scope::dc_scope::monotonic_seconds()),"Actual GPU scope copy failed");
    auto masked=gpu.scope_widget(scope,color,identity,true);require(masked.empty(),"Authorized physical scope also rendered native widget");
    owned.invalidate();auto unavailable=gpu.scope_widget(scope,color,identity,true);for(const auto& v:unavailable)close(v.w,2.5f,"Unavailable GPU scope did not fall back to HUD plane");
    require(unavailable.size()==92*3,"Unavailable GPU scope lost native widget");require(owned.release(identity),"Scope GPU test teardown failed");
    glDeleteTextures(1,&source_texture);glDeleteFramebuffers(1,&source_fbo);panel_metres=1.8f;
    config::VrReproject=false;xr_eye=false;auto baseline=gpu.draw(ctx,pp,original,true,false);const auto pixels=gpu.pixels();
    auto unchanged=gpu.draw(ctx,pp,original,true,true);require(baseline.size()==unchanged.size()&&std::memcmp(baseline.data(),unchanged.data(),baseline.size()*sizeof(glm::vec4))==0,"Native Original vertex output changed");
    require(pixels==gpu.pixels(),"Native Original color output changed");bool covered=false;for(auto byte:pixels)covered=covered||byte!=0;require(covered,"Native Original comparison was empty");
    auto native_scope=gpu.scope_widget(scope,original,identity,false,false);const auto original_scope_pixels=gpu.pixels();
    auto original_with_adapter=gpu.scope_widget(scope,original,identity,false,true);
    require(native_scope.size()==original_with_adapter.size()&&std::memcmp(native_scope.data(),original_with_adapter.data(),native_scope.size()*sizeof(glm::vec4))==0,"Native Original scope positions changed");
    require(original_scope_pixels==gpu.pixels(),"Native Original scope RGBA changed");
    auto native_targets=gpu.draw(indicators,target_p,original,true,false);const auto native_target_pixels=gpu.pixels();
    auto preserved_targets=gpu.draw(indicators,target_p,original,true,true);
    require(native_targets.size()==preserved_targets.size()&&std::memcmp(native_targets.data(),preserved_targets.data(),native_targets.size()*sizeof(glm::vec4))==0,"Native Original target positions changed");
    require(native_target_pixels==gpu.pixels(),"Native Original target RGBA changed");
    auto native_message=gpu.scope_widget(advisory,original,identity,false,false);const auto native_message_rgba=gpu.pixels();
    auto preserved_message=gpu.scope_widget(advisory,original,identity,false,true);
    require(native_message.size()==preserved_message.size()&&std::memcmp(native_message.data(),preserved_message.data(),native_message.size()*sizeof(glm::vec4))==0,"Native Original advisory positions changed");
    require(native_message_rgba==gpu.pixels(),"Native Original advisory RGBA changed");
    auto native_primer=gpu.draw(primer,primer_p,original,true,false);auto preserved_primer=gpu.draw(primer,primer_p,original,true,true);
    require(native_primer.size()==6&&native_primer.size()==preserved_primer.size()&&std::memcmp(native_primer.data(),preserved_primer.data(),native_primer.size()*sizeof(glm::vec4))==0,"Native Original list primer changed");
    glDeleteProgram(color.program);glDeleteProgram(depth.program);glDeleteProgram(original.program);require(glGetError()==GL_NO_ERROR,"Final OpenGL error");
    std::cout<<"Verbatim native shader "<<hud_gpu_fixture::VertexSha256<<"; hook "<<hud_gpu_fixture::HookSha256<<"; depth hook "<<hud_gpu_fixture::DepthHookSha256<<"\nHUD GL: physical distance, native scale1/3, saved size gate, mixed world/reset, color/depth, strip/restart, Native Original passed\nScope fallback: full26proof, physical plane, scale1/3, size gate, depth, fresh-GPU suppression, unavailable-image fallback, Original RGBA preserved\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"HUD GL failure: "<<e.what()<<'\n';return 1;}}
