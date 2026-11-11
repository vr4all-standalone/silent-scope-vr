// SPDX-License-Identifier: GPL-2.0-or-later
#include "rifle_renderer.h"
#include "rifle_simulation.h"
#ifdef SILENT_SCOPE_FLYCAST_GL
#include "wsi/gl_context.h"
#else
#include <glad/gl.h>
#endif
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace silent_scope {
namespace {
#ifdef __ANDROID__
#define SS_GLSL "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
#define SS_GLSL "#version 330 core\n"
#endif
struct Vertex {float x,y,z,u,v,r,g,b,a;};
GLuint shader(GLenum type,const char* code) {
    auto value=glCreateShader(type);glShaderSource(value,1,&code,nullptr);glCompileShader(value);
    GLint good{};glGetShaderiv(value,GL_COMPILE_STATUS,&good);
    if(!good){char message[2048]{};glGetShaderInfoLog(value,sizeof(message),nullptr,message);glDeleteShader(value);throw std::runtime_error(message);}
    return value;
}
struct Texture {
    GLuint name{};uint64_t generation{},sequence{};
    bool upload(const std::shared_ptr<const RasterFrame>& frame) {
        if(!frame||!frame->width||!frame->height||frame->width>2048||frame->height>2048||
           uint64_t(frame->width)*frame->height!=frame->pixels.size()) {generation=sequence=0;return false;}
        if(name&&generation==frame->generation&&sequence==frame->sequence)return true;
        std::vector<uint8_t> rgba;rgba.reserve(frame->pixels.size()*4);
        for(auto argb:frame->pixels){rgba.push_back(argb>>16);rgba.push_back(argb>>8);rgba.push_back(argb);rgba.push_back(255);}
        if(!name)glGenTextures(1,&name);
        glBindTexture(GL_TEXTURE_2D,name);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,frame->width,frame->height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        generation=frame->generation;sequence=frame->sequence;return true;
    }
    ~Texture(){if(name)glDeleteTextures(1,&name);}
};
}
struct RifleRenderer::Impl {
    GLuint program{},vao{},buffer{},body_buffer{},bolt_buffer{},magazine_buffer{};
    GLint matrix{},textured{},highlighted{};
    Texture main,scope;
    Vec3 ocular;
    double aperture_radius;
    double image_z;
    GLsizei body_count, bolt_count, magazine_count;
    explicit Impl(const RifleMesh& rifle)
        : ocular(rifle.ocular), aperture_radius(rifle.aperture_radius),
          image_z(rifle.ocular.z-rifle.image_inset),
          body_count(static_cast<GLsizei>(rifle.vertices.size())),
          bolt_count(static_cast<GLsizei>(rifle.bolt.size())), magazine_count(static_cast<GLsizei>(rifle.magazine.size())) {
        const char* vertex=SS_GLSL R"(
layout(location=0) in vec3 position;layout(location=1) in vec2 input_uv;layout(location=2) in vec4 input_color;
uniform mat4 matrix;out vec2 uv;out vec4 color;
void main(){gl_Position=matrix*vec4(position,1);uv=input_uv;color=input_color;})";
        const char* fragment=SS_GLSL R"(
in vec2 uv;in vec4 color;uniform sampler2D image;uniform int textured;uniform int highlighted;out vec4 pixel;
void main(){
    pixel=color*(textured!=0?texture(image,uv):vec4(1));
    if(highlighted!=0)pixel.rgb=mix(pixel.rgb,vec3(1.0,0.86,0.05),0.85);
})";
        auto vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
        glDeleteShader(vs);glDeleteShader(fs);GLint good{};glGetProgramiv(program,GL_LINK_STATUS,&good);
        if(!good)throw std::runtime_error("Rifle shader link failed");
        matrix=glGetUniformLocation(program,"matrix");textured=glGetUniformLocation(program,"textured");
        highlighted=glGetUniformLocation(program,"highlighted");
        glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);
        upload(rifle.vertices,body_buffer);
        upload(rifle.bolt,bolt_buffer);
        upload(rifle.magazine,magazine_buffer);
    }
    static void upload(const std::vector<RifleVertex>& mesh, GLuint& target) {
        if(mesh.empty())return;
        std::vector<Vertex> body;
        body.reserve(mesh.size());
        for(const auto& vertex:mesh)body.push_back({float(vertex.position.x),float(vertex.position.y),float(vertex.position.z),0,0,
            vertex.color[0],vertex.color[1],vertex.color[2],vertex.color[3]});
        glGenBuffers(1,&target);
        glBindBuffer(GL_ARRAY_BUFFER,target);
        glBufferData(GL_ARRAY_BUFFER,body.size()*sizeof(Vertex),body.data(),GL_STATIC_DRAW);
    }
    ~Impl(){glDeleteBuffers(1,&bolt_buffer);glDeleteBuffers(1,&magazine_buffer);glDeleteBuffers(1,&body_buffer);glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);}
    void draw_buffer(GLuint source,GLsizei count,const glm::mat4& transform,GLuint texture=0,bool translucent=false,bool highlight=false) {
        glEnable(GL_DEPTH_TEST);glDepthMask(translucent?GL_FALSE:GL_TRUE);
        if(translucent){glEnable(GL_CULL_FACE);glEnable(GL_BLEND);}
        else{glDisable(GL_CULL_FACE);glDisable(GL_BLEND);}
        glUseProgram(program);glUniformMatrix4fv(matrix,1,GL_FALSE,glm::value_ptr(transform));glUniform1i(textured,texture!=0);
        glUniform1i(highlighted,highlight);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glUniform1i(glGetUniformLocation(program,"image"),0);
        glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,source);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(5*sizeof(float)));
        glDrawArrays(GL_TRIANGLES,0,count);glBindVertexArray(0);
    }
    void draw(const std::vector<Vertex>& vertices,const glm::mat4& transform,GLuint texture=0,bool translucent=false) {
        glBindBuffer(GL_ARRAY_BUFFER,buffer);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
        draw_buffer(buffer,static_cast<GLsizei>(vertices.size()),transform,texture,translucent);
    }
};
RifleRenderer::RifleRenderer():RifleRenderer(make_rifle_mesh()){}
RifleRenderer::RifleRenderer(const RifleMesh& mesh):impl_(std::make_unique<Impl>(mesh)){}
RifleRenderer::~RifleRenderer()=default;
void RifleRenderer::draw_magazine(const glm::mat4& vp,const glm::mat4& model,bool highlighted) {
    if (impl_->magazine_count)
        impl_->draw_buffer(impl_->magazine_buffer,impl_->magazine_count,vp*model,0,false,highlighted);
}
void RifleRenderer::draw_support_box(const glm::mat4& vp,const glm::mat4& model,bool guide) {
    const auto make_box=[](bool yellow) {
        std::vector<Vertex> mesh;
        const int faces[][6]={{0,2,3,0,3,1},{4,5,7,4,7,6},{0,1,5,0,5,4},
            {2,6,7,2,7,3},{0,4,6,0,6,2},{1,3,7,1,7,5}};
        for (const auto& face:faces) {
            for (const int corner:face) {
                mesh.push_back({corner&1?.032f:-.032f,corner&2?.022f:-.022f,
                    corner&4?.048f:-.048f,0,0,1.f,yellow?.86f:.06f,.06f,yellow?.48f:.32f});
            }
        }
        return mesh;
    };
    static const auto redBox=make_box(false), yellowBox=make_box(true);
    GLint srcRgb,dstRgb,srcAlpha,dstAlpha,eqRgb,eqAlpha,cull,front;
    glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
    glGetIntegerv(GL_CULL_FACE_MODE,&cull);glGetIntegerv(GL_FRONT_FACE,&front);
    glCullFace(GL_BACK);glFrontFace(GL_CCW);
    glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    impl_->draw(guide?yellowBox:redBox,vp*model,0,true);
    glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
    glBlendEquationSeparate(eqRgb,eqAlpha);
    glCullFace(cull);glFrontFace(front);
}
void RifleRenderer::draw_cartridge(const glm::mat4& vp,const glm::mat4& model,bool spent) {
    const auto make_mesh=[](bool casing_only){
        std::vector<Vertex> mesh;
        const auto ring=[](float angle,float radius,float z,float r,float g,float b){
            return Vertex{radius*std::cos(angle),radius*std::sin(angle),z,0,0,r,g,b,1};
        };
        for(int i=0;i<12;++i){
            const float a=i*glm::two_pi<float>()/12, b=(i+1)*glm::two_pi<float>()/12;
            const auto baseA=ring(a,.006f,.023f,.82f,.59f,.22f),baseB=ring(b,.006f,.023f,.82f,.59f,.22f);
            const auto mouthA=ring(a,.0045f,-.022f,.96f,.72f,.28f),mouthB=ring(b,.0045f,-.022f,.96f,.72f,.28f);
            mesh.insert(mesh.end(),{baseA,baseB,mouthA,mouthA,baseB,mouthB,
                baseA,ring(0,0,.023f,.65f,.46f,.16f),baseB});
            const auto tip= casing_only ? ring(0,0,-.014f,.17f,.12f,.06f) : ring(0,0,-.041f,.77f,.37f,.19f);
            auto edgeA=mouthA,edgeB=mouthB;
            if(!casing_only){edgeA.r=edgeB.r=.63f;edgeA.g=edgeB.g=.28f;edgeA.b=edgeB.b=.13f;}
            mesh.insert(mesh.end(),{edgeA,edgeB,tip});
        }
        return mesh;
    };
    static const auto cartridge=make_mesh(false), casing=make_mesh(true);
    impl_->draw(spent?casing:cartridge,vp*model);
}
void RifleRenderer::draw_panel(const glm::mat4& vp,std::shared_ptr<const RasterFrame> frame,float distance) {
    if(!impl_->main.upload(frame)||!std::isfinite(distance)||distance<.5f||distance>10.f)return;
    if(!std::isfinite(frame->aspect)||frame->aspect<.25||frame->aspect>4)return;
    float half_height=distance*.45f,half_width=half_height*frame->aspect,z=-distance;
    Vertex tl{-half_width,half_height,z,0,0,1,1,1,1},tr{half_width,half_height,z,1,0,1,1,1,1};
    Vertex bl{-half_width,-half_height,z,0,1,1,1,1,1},br{half_width,-half_height,z,1,1,1,1,1,1};
    impl_->draw({tl,bl,br,tl,br,tr},vp,impl_->main.name);
}
void RifleRenderer::draw_rifle(const glm::mat4& vp,const glm::mat4& model,glm::vec3 eye,std::shared_ptr<const RasterFrame> scope) {
    const bool uploaded=impl_->scope.upload(scope);
    draw_rifle_texture(vp,model,eye,uploaded?impl_->scope.name:0,scope?float(scope->aspect):1.f,false);
}
void RifleRenderer::draw_rifle_texture(const glm::mat4& vp,const glm::mat4& model,glm::vec3 eye,uint32_t scope_texture,float aspect,bool bottom_left,float image_offset,float image_scale,
    float bolt_offset,const glm::mat4* magazine_model,bool magazine_visible,float bolt_lift,bool highlight_bolt,bool highlight_magazine) {
    impl_->draw_buffer(impl_->body_buffer,impl_->body_count,vp*model);
    if(impl_->bolt_count){
        const auto axis=RifleSimulation::BoltAxis;
        auto bolt=glm::translate(model,axis+glm::vec3(0,0,bolt_offset));
        bolt=glm::rotate(bolt,bolt_lift,glm::vec3(0,0,1));
        impl_->draw_buffer(impl_->bolt_buffer,impl_->bolt_count,vp*glm::translate(bolt,-axis),0,false,highlight_bolt);
    }
    if(impl_->magazine_count&&magazine_visible)
        impl_->draw_buffer(impl_->magazine_buffer,impl_->magazine_count,vp*(magazine_model?*magazine_model:model),0,false,highlight_magazine);
    const auto ocular=impl_->ocular;
    auto local=glm::inverse(model)*glm::vec4(eye,1);
    auto visibility=scope_visibility({local.x-ocular.x,local.y-ocular.y,local.z-ocular.z},{});
    bool show=scope_texture!=0&&visibility.visible;
    std::vector<Vertex> lens;
    auto vertex=[&](double x,double y){
        auto uv=scope_uv({x,y},aspect);
        if(bottom_left)uv.v=1-uv.v;
        if(!uv.valid)show=false;
        return Vertex{float(ocular.x+x*2*impl_->aperture_radius*image_scale),float(ocular.y+y*2*impl_->aperture_radius*image_scale),
                      float(impl_->image_z+image_offset),float(uv.u),float(uv.v),1,1,1,1};
    };
    for(int i=0;i<48;++i){
        constexpr double pi=3.14159265358979323846;
        double a=2*pi*i/48,b=2*pi*(i+1)/48;
        lens.push_back(vertex(0,0));lens.push_back(vertex(.5*std::cos(a),.5*std::sin(a)));lens.push_back(vertex(.5*std::cos(b),.5*std::sin(b)));
    }
    if(!show)for(auto& v:lens)v.r=v.g=v.b=.01f;
    impl_->draw(lens,vp*model,show?scope_texture:0);
}
}
