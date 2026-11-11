#include <glm/gtc/matrix_transform.hpp>
// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_scene.h"
#include "scene_shaders.h"
#include "scene_geometry.h"
#include "texture_hash.h"

#include "background_surface.h"
#include "background_occlusion.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
namespace silent_scope::scene {
static std::string source(const char* text) {
    std::string out=text;
#ifdef __ANDROID__
    out.replace(0,out.find('\n'),"#version 300 es\nprecision highp float;\nprecision highp int;");
#endif
    return out;
}
static GLuint shader(GLenum kind,const char* text) {
    auto s=source(text);const char* p=s.c_str();auto shader=glCreateShader(kind);
    glShaderSource(shader,1,&p,nullptr);glCompileShader(shader);
    GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(log); }
    return shader;
}
struct GLScene::Impl {
    GLuint program{},vao{},buffer{},white{};
    GLuint samplers[8]{};
    GLuint background_program{},background_vao{},background_buffer{},background_elements{},background_texture{};
    BackgroundSurface surface;bool depth_background{};
    GLuint native_depth{};bool has_native_depth{};
    struct Texture {GLuint name{};uint32_t width{},height{},lod{};uint64_t used{};};
    std::map<uint64_t,Texture> textures;
    FrameTextureHashes frame_hashes;
    std::map<std::string,GLint,std::less<>> uniforms;
    struct Draw {RawTriangle triangle;GLuint texture{};float tw{1},th{1};bool panel{},background{};GLint first{};GLsizei count{3};};
    std::vector<Draw> draws;
    std::shared_ptr<const RawFrame> frame;
    uint64_t age{};
    GLint uniform(const char* name){auto found=uniforms.find(name);if(found!=uniforms.end())return found->second;auto location=glGetUniformLocation(program,name);uniforms.emplace(name,location);return location;}
    void color(const char* name,uint32_t c) {glUniform4f(uniform(name),(c>>16)&255,(c>>8)&255,c&255,c>>24);}
    GLuint texture(const TextureImage& image) {
        const auto hash=frame_hashes.get(image);
        auto& t=textures[hash];t.used=age;
        if(!t.name) {
            t.width=image.width;t.height=image.height;t.lod=image.lod;
            std::vector<uint8_t> rgba(image.pixels.size()*4);
            for(size_t i=0;i<image.pixels.size();i++) {auto p=image.pixels[i];rgba[i*4]=p>>16;rgba[i*4+1]=p>>8;rgba[i*4+2]=p;rgba[i*4+3]=p>>24;}
            glGenTextures(1,&t.name);glBindTexture(GL_TEXTURE_2D,t.name);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        }
        return t.name;
    }
    void add(const RawTriangle& tri,bool panel,bool background=false) {
        if(tri.texture[1] || ((tri.state.fog_mode&1)&&!(tri.state.fog_mode&32)&&(tri.state.fog_mode&24)))throw std::runtime_error("Uncaptured Voodoo fog/second TMU state");
        Draw draw{tri,white,1,1,panel,background};
        draw.first=GLint(draws.size()*3);
        if(auto t=tri.texture[0]) {draw.texture=texture(*t);draw.tw=t->width*(1u<<t->lod);draw.th=t->height*(1u<<t->lod);}
        draws.push_back(std::move(draw));
    }
    void draw_background(const glm::mat4& vp,float scale,const NativeLens& lens,uint32_t width,uint32_t height) {
        if(!surface.valid())return;
        glUseProgram(background_program);glBindVertexArray(background_vao);
        glUniformMatrix4fv(glGetUniformLocation(background_program,"vp"),1,GL_FALSE,glm::value_ptr(vp));
        glUniform4f(glGetUniformLocation(background_program,"lens"),lens.fx,lens.fy,lens.cx,lens.cy);
        glUniform1f(glGetUniformLocation(background_program,"scale"),scale);
        glUniform2f(glGetUniformLocation(background_program,"size"),width,height);
        glUniform1i(glGetUniformLocation(background_program,"image"),0);glBindTexture(GL_TEXTURE_2D,background_texture);
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDrawElements(GL_TRIANGLES,GLsizei(surface.indices.size()),GL_UNSIGNED_INT,nullptr);
        glUseProgram(program);glBindVertexArray(vao);
    }
};
GLScene::GLScene():impl_(std::make_unique<Impl>()) {
    auto& p=*impl_;
    GLuint vs=shader(GL_VERTEX_SHADER,SceneVert),fs=shader(GL_FRAGMENT_SHADER,SceneFrag);
    p.program=glCreateProgram();glAttachShader(p.program,vs);glAttachShader(p.program,fs);
    glBindAttribLocation(p.program,0,"in_position");glBindAttribLocation(p.program,1,"in_tex");glBindAttribLocation(p.program,2,"in_color");
    glLinkProgram(p.program);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok{};glGetProgramiv(p.program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Voodoo shader link failed");
    glGenVertexArrays(1,&p.vao);glBindVertexArray(p.vao);glGenBuffers(1,&p.buffer);glBindBuffer(GL_ARRAY_BUFFER,p.buffer);
    for(int i=0;i<3;i++) {glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==2?4:3,GL_FLOAT,GL_FALSE,40,reinterpret_cast<void*>(uintptr_t(i==0?0:i==1?12:24)));}
    glGenTextures(1,&p.white);glBindTexture(GL_TEXTURE_2D,p.white);uint32_t white=0xffffffff;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);
    glGenSamplers(8,p.samplers);
    for(int i=0;i<8;++i){glSamplerParameteri(p.samplers[i],GL_TEXTURE_WRAP_S,i&1?GL_CLAMP_TO_EDGE:GL_REPEAT);glSamplerParameteri(p.samplers[i],GL_TEXTURE_WRAP_T,i&2?GL_CLAMP_TO_EDGE:GL_REPEAT);glSamplerParameteri(p.samplers[i],GL_TEXTURE_MIN_FILTER,i&4?GL_LINEAR:GL_NEAREST);glSamplerParameteri(p.samplers[i],GL_TEXTURE_MAG_FILTER,i&4?GL_LINEAR:GL_NEAREST);}

    const char* background_vertex=R"(#version 330
in vec3 raster;
uniform mat4 vp;uniform vec4 lens;uniform float scale;
out vec3 source;
void main(){float z=1.0/raster.z;vec3 camera=vec3((raster.x-lens.z)/lens.x*z,(lens.w-raster.y)/lens.y*z,-z);
gl_Position=vp*vec4(camera*scale,1.0);source=vec3(raster.xy,1.0)*z;})";
    const char* background_fragment=R"(#version 330
in vec3 source;uniform sampler2D image;uniform vec2 size;out vec4 color;
void main(){color=texture(image,source.xy/source.z/size);})";
    vs=shader(GL_VERTEX_SHADER,background_vertex);fs=shader(GL_FRAGMENT_SHADER,background_fragment);
    p.background_program=glCreateProgram();glAttachShader(p.background_program,vs);glAttachShader(p.background_program,fs);
    glBindAttribLocation(p.background_program,0,"raster");glLinkProgram(p.background_program);glDeleteShader(vs);glDeleteShader(fs);
    glGetProgramiv(p.background_program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Background shader link failed");
    glGenVertexArrays(1,&p.background_vao);glBindVertexArray(p.background_vao);
    glGenBuffers(1,&p.background_buffer);glBindBuffer(GL_ARRAY_BUFFER,p.background_buffer);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(BackgroundVertex),nullptr);
    glGenBuffers(1,&p.background_elements);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p.background_elements);
}
GLScene::~GLScene() {
    glDeleteSamplers(8,impl_->samplers);
    auto& p=*impl_;for(auto& [key,t]:p.textures)glDeleteTextures(1,&t.name);
    glDeleteTextures(1,&p.white);glDeleteBuffers(1,&p.buffer);glDeleteVertexArrays(1,&p.vao);glDeleteProgram(p.program);
    glDeleteBuffers(1,&p.background_buffer);glDeleteBuffers(1,&p.background_elements);glDeleteVertexArrays(1,&p.background_vao);glDeleteProgram(p.background_program);
    glDeleteTextures(1,&p.native_depth);
}
void GLScene::prepare(std::shared_ptr<const RawFrame> frame,bool depth_background) {
    auto& p=*impl_;if(p.frame==frame&&p.depth_background==depth_background)return;
    p.depth_background=depth_background;p.surface={};p.has_native_depth=false;
    p.frame_hashes.reset();p.frame=std::move(frame);p.draws.clear();++p.age;
    if(!p.frame || !p.frame->complete)return;
    const auto& f=*p.frame;
    if(f.lens.valid()&&!f.background_depth.empty()&&f.background_depth.size()==size_t(f.width)*f.height) {
        if(!p.native_depth)glGenTextures(1,&p.native_depth);
        glBindTexture(GL_TEXTURE_2D,p.native_depth);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R16UI,f.width,f.height,0,GL_RED_INTEGER,GL_UNSIGNED_SHORT,f.background_depth.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT,4);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        p.has_native_depth=true;
    }
    if(depth_background)p.surface=BackgroundSurface::build(f);
    if(p.surface.valid()) {
        glBindVertexArray(p.background_vao);glBindBuffer(GL_ARRAY_BUFFER,p.background_buffer);
        glBufferData(GL_ARRAY_BUFFER,p.surface.vertices.size()*sizeof(BackgroundVertex),p.surface.vertices.data(),GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,p.background_elements);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,p.surface.indices.size()*sizeof(uint32_t),p.surface.indices.data(),GL_STREAM_DRAW);
    }
    if(!f.background.empty()) {
        // Place original bitmap artwork behind all captured world models.
        double far_units=f.lens.valid()?background_units(f):1.1;
        if(p.surface.valid())far_units=std::max(far_units,p.surface.far_units()*1.05);
        auto bitmap=std::make_shared<const TextureImage>(0,f.sequence,f.width,f.height,f.background);
        RawVertex vertices[4];const double xy[4][2]={{0,0},{double(f.width),0},{0,double(f.height)},{double(f.width),double(f.height)}};
        for(int i=0;i<4;i++) {auto& v=vertices[i];v.x=xy[i][0];v.y=xy[i][1];v.q=1./far_units;v.s[0]=v.x*v.q;v.t[0]=v.y*v.q;v.tq[0]=v.q;v.r=v.g=v.b=v.a=255;}
        for(auto ids:{std::array<int,3>{0,1,2},std::array<int,3>{2,1,3}}) {
            RawTriangle t;t.state.color_path=5;t.state.depth_mode=512;t.state.texture_mode=0x082410c5;t.texture[0]=bitmap;
            for(int i=0;i<3;i++)t.vertex[i]=vertices[ids[i]];p.add(t,!f.lens.valid(),true);
        }
        p.background_texture=p.draws.front().texture;
    }
    if(f.lens.valid())for(const auto& tri:f.triangles) {
        p.add(tri,tri.overlay||scene_panel_triangle(tri));
    }
    std::vector<float> data;data.reserve(p.draws.size()*30);
    for(const auto& draw:p.draws)for(const auto& v:draw.triangle.vertex)for(double n:{v.x,v.y,v.q,v.s[0],v.t[0],v.tq[0],v.r,v.g,v.b,v.a})data.push_back(float(n));
    glBindVertexArray(p.vao);glBindBuffer(GL_ARRAY_BUFFER,p.buffer);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_STREAM_DRAW);
    // Preserve native primitive order while combining consecutive triangles that
    // share every GPU state. This avoids one mobile draw call per triangle.
    std::vector<Impl::Draw> batched;batched.reserve(p.draws.size());
    for(auto& draw:p.draws) {
        if(!batched.empty()) {
            auto& previous=batched.back();
            if(previous.texture==draw.texture&&previous.panel==draw.panel&&previous.background==draw.background&&previous.tw==draw.tw&&previous.th==draw.th&&
               previous.triangle.clip_left==draw.triangle.clip_left&&previous.triangle.clip_right==draw.triangle.clip_right&&previous.triangle.clip_top==draw.triangle.clip_top&&previous.triangle.clip_bottom==draw.triangle.clip_bottom&&
               previous.triangle.depth_constant==draw.triangle.depth_constant&&previous.triangle.fog_table==draw.triangle.fog_table&&previous.triangle.fog_delta_mask==draw.triangle.fog_delta_mask&&
               std::memcmp(&previous.triangle.state,&draw.triangle.state,sizeof(RenderState))==0) {previous.count+=draw.count;continue;}
        }
        batched.push_back(std::move(draw));
    }
    p.draws=std::move(batched);
    for(auto i=p.textures.begin();i!=p.textures.end();)if(i->second.used+2<p.age) {glDeleteTextures(1,&i->second.name);i=p.textures.erase(i);}else ++i;
}
const BackgroundSurface* GLScene::depth_surface(const std::shared_ptr<const RawFrame>& frame,bool depth_background) const {
    const auto& p=*impl_;
    return depth_background&&p.depth_background&&p.frame==frame&&p.surface.valid()?&p.surface:nullptr;
}

void GLScene::draw(const glm::mat4& vp,float scale,float panel_distance,float depth_strength,const StereoPresentation& stereo,float hud_size) {
    auto& p=*impl_;if(!p.frame || !p.frame->complete)return;
    const auto& f=*p.frame;
    hud_size=f.lens.valid()&&std::isfinite(hud_size)&&hud_size>=.5f&&hud_size<=2.f?hud_size:1.f;
    glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    auto c=f.clear_color;glClearColor(((c>>16)&255)/255.f,((c>>8)&255)/255.f,(c&255)/255.f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(p.program);glBindVertexArray(p.vao);glActiveTexture(GL_TEXTURE0);
    glUniform1i(p.uniform("u_native_background_depth"),1);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,p.native_depth);glActiveTexture(GL_TEXTURE0);
    glUniform1i(p.uniform("u_texture"),0);glUniform1i(p.uniform("u_spatial"),1);
    glUniform1f(p.uniform("u_eye_shift"),0);glUniform2f(p.uniform("u_size"),f.width,f.height);
    auto lens=presentation_lens(f);glUniform4f(p.uniform("u_lens"),lens.fx,lens.fy,lens.cx,lens.cy);
    glUniform1f(p.uniform("u_world_scale"),scale);glUniform1f(p.uniform("u_panel_distance"),panel_distance);
    auto world_vp=vp;const double gain=f.lens.valid()?stereo.factor():1;
    if(gain!=1){const glm::vec3 c(float(stereo.center.x),float(stereo.center.y),float(stereo.center.z));world_vp=vp*glm::translate(glm::mat4(1),c)*glm::scale(glm::mat4(1),glm::vec3(float(1/gain)))*glm::translate(glm::mat4(1),-c);}
    world_vp[2]/=float(scene_depth_strength(depth_strength));
    bool panel_projection=false;
    glUniformMatrix4fv(p.uniform("u_view_projection"),1,GL_FALSE,glm::value_ptr(world_vp));
    const GLenum compare[]={GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};
    const GLenum blend[]={GL_ZERO,GL_SRC_ALPHA,GL_DST_COLOR,GL_DST_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ONE_MINUS_DST_COLOR,GL_ONE_MINUS_DST_ALPHA,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR};
    auto factor=[&](uint32_t v,GLenum fallback){return v<10?blend[v]:fallback;};
    std::array<uint16_t,64> previous_fog{};bool fog_uploaded=false;
    const Impl::Draw* previous=nullptr;int previous_sampler=-1;
    for(size_t i=0;i<p.draws.size();i++) {
        const auto& d=p.draws[i];const auto* prior=previous?&previous->triangle:nullptr;

        if(d.panel!=panel_projection){glUniformMatrix4fv(p.uniform("u_view_projection"),1,GL_FALSE,glm::value_ptr(d.panel?vp:world_vp));panel_projection=d.panel;}
        const auto& s=d.triangle.state;auto fbz=s.depth_mode,a=s.alpha_mode,m=s.texture_mode;
        const auto& clip=d.triangle;
        // Zero bounds identify generated HUD/background primitives, whose
        // rectangle is the full native image. Native bounds are half-open.
        bool has_clip=clip.clip_right>clip.clip_left&&clip.clip_bottom>clip.clip_top;
        if(!prior||clip.clip_left!=prior->clip_left||clip.clip_right!=prior->clip_right||clip.clip_top!=prior->clip_top||clip.clip_bottom!=prior->clip_bottom) glUniform4f(p.uniform("u_native_clip"),has_clip?clip.clip_left:0,has_clip?clip.clip_top:0,has_clip?clip.clip_right:f.width,has_clip?clip.clip_bottom:f.height);
        glUniform1i(p.uniform("u_test_background_depth"),p.has_native_depth&&!d.background&&native_background_depth_supported(d.triangle));
        if(!prior||clip.depth_constant!=prior->depth_constant) glUniform1ui(p.uniform("u_native_depth_constant"),uint16_t(d.triangle.depth_constant));
        if(!previous||d.panel!=previous->panel||d.background!=previous->background) glUniform1f(p.uniform("u_hud_size"),d.panel&&!d.background?hud_size:1.f);
        if(!previous||d.panel!=previous->panel) glUniform1i(p.uniform("u_panel"),d.panel);
        if(!prior||s.depth_mode!=prior->state.depth_mode) glUniform1i(p.uniform("u_y_flip"),(fbz>>17)&1);
        if(!prior||s.color_path!=prior->state.color_path) glUniform1ui(p.uniform("u_color_path"),s.color_path);
        if(!prior||s.depth_mode!=prior->state.depth_mode) glUniform1ui(p.uniform("u_depth_mode"),fbz);
        if(!prior||s.alpha_mode!=prior->state.alpha_mode) glUniform1ui(p.uniform("u_alpha_mode"),a);
        if(!prior||s.texture_mode!=prior->state.texture_mode) glUniform1ui(p.uniform("u_texture_mode"),m);
        if(!prior||s.fog_mode!=prior->state.fog_mode) glUniform1ui(p.uniform("u_fog_mode"),s.fog_mode);
        if(!prior||clip.fog_delta_mask!=prior->fog_delta_mask) glUniform1ui(p.uniform("u_fog_delta_mask"),d.triangle.fog_delta_mask);
        if(!prior||s.fog_color!=prior->state.fog_color) p.color("u_fog_color",s.fog_color);
        if((s.fog_mode&1)&&(!fog_uploaded||previous_fog!=d.triangle.fog_table)) {
            std::array<GLuint,64> values{};for(size_t j=0;j<64;j++)values[j]=d.triangle.fog_table[j];
            glUniform1uiv(p.uniform("u_fog_table[0]"),64,values.data());previous_fog=d.triangle.fog_table;fog_uploaded=true;
        }
        if(!prior||s.color0!=prior->state.color0) p.color("u_color0",s.color0);
        if(!prior||s.color1!=prior->state.color1) p.color("u_color1",s.color1);
        if(!prior||s.chroma_key!=prior->state.chroma_key) p.color("u_chroma_key",s.chroma_key);
        if(!prior||bool(clip.texture[0])!=bool(prior->texture[0])) glUniform1i(p.uniform("u_textured"),bool(d.triangle.texture[0]));
        if(!previous||d.tw!=previous->tw||d.th!=previous->th) glUniform2f(p.uniform("u_texture_size"),d.tw,d.th);
        glBindTexture(GL_TEXTURE_2D,d.texture);
        int sampler=((m&64)?1:0)|((m&128)?2:0)|((m&6)?4:0);
        if(sampler!=previous_sampler){glBindSampler(0,p.samplers[sampler]);previous_sampler=sampler;}
        if((fbz&16) && !d.panel) {glEnable(GL_DEPTH_TEST);glDepthFunc(compare[(fbz>>5)&7]);}else glDisable(GL_DEPTH_TEST);
        glDepthMask(bool(fbz&1024)&&!d.panel);glColorMask(bool(fbz&512),bool(fbz&512),bool(fbz&512),bool(fbz&512));
        if(a&16) {glEnable(GL_BLEND);glBlendFuncSeparate(factor((a>>8)&15,GL_ONE),factor((a>>12)&15,GL_ZERO),factor((a>>16)&15,GL_ONE),factor((a>>20)&15,GL_ZERO));}else glDisable(GL_BLEND);
        glDrawArrays(GL_TRIANGLES,d.first,d.count);
        if(d.background)p.draw_background(world_vp,scale,lens,f.width,f.height);
        previous=d.background?nullptr:&d;
    }
    // Rifle rendering uses its own texture-object parameters on this unit.
    glBindSampler(0,0);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
}
}

