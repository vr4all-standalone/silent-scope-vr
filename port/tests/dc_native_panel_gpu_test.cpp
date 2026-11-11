// SPDX-License-Identifier: GPL-2.0-or-later
// Real GL regression using the production native raster/overlay vertex shader;
// actual captured native-warning triangle, evaluated by desktop OpenGL.
#include "gl_context.h"
#include <glad/gl.h>
#include "dreamcast/dc_native_panel.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
static std::string read(const char* p){std::ifstream f(p,std::ios::binary);require(bool(f),"Missing source");std::string s((std::istreambuf_iterator<char>(f)),{}),lf;for(char c:s)if(c!='\r')lf+=c;return lf;}
static GLuint shader(GLenum type,const std::string& text){GLuint s=glCreateShader(type);const char* p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok{};glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096]{};glGetShaderInfoLog(s,sizeof log,nullptr,log);throw std::runtime_error(log);}return s;}
// Exact production vr_reproject.cpp projection() function; runtime source check below.
static glm::mat4 projection(glm::vec2 tanHalf,float zNear){glm::mat4 proj(0.f);proj[0][0]=1.f/tanHalf.x;proj[1][1]=1.f/tanHalf.y;proj[2][2]=-1.f;proj[2][3]=-1.f;proj[3][2]=-2.f*zNear;return proj;}
static glm::mat4 eye(float l,float r){glm::mat4 p(0);p[0][0]=2/(r-l);p[1][1]=1;p[2][0]=(r+l)/(r-l);p[2][2]=-1;p[2][3]=-1;p[3][2]=-.1f;return p;}
// Production vertex shader + verbatim production fragment clip, rasterized by
// real GL. The white output isolates coverage; no substitute clip math runs in
// the shader. CPU projection is used only to classify read-back pixel locations.
static void canvas_raster(GLuint vs,GLuint vao,GLuint vbo,const std::string& clip,const glm::mat4& ndc,
                          const glm::mat4& vp,glm::vec4 overlay,glm::vec2 tanHalf){
    const auto fs=shader(GL_FRAGMENT_SHADER,"#version 330 core\n#define VR_REPROJECT 1\nin highp vec3 vtx_native_canvas;\nout vec4 color;\nvoid main(){\n"+clip+"\ncolor=vec4(1);}\n");
    GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glBindAttribLocation(program,0,"in_pos");
    const char* nativeDepth="vtx_uv";glTransformFeedbackVaryings(program,1,&nativeDepth,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);
    GLint linked{};glGetProgramiv(program,GL_LINK_STATUS,&linked);require(linked,"Production native-canvas clip program failed to link");glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program,"ndcMat"),1,GL_FALSE,&ndc[0][0]);glUniformMatrix4fv(glGetUniformLocation(program,"vrMat"),1,GL_FALSE,&vp[0][0]);
    glUniform2fv(glGetUniformLocation(program,"vrTan"),1,&tanHalf[0]);glUniform3f(glGetUniformLocation(program,"vrComfort"),0,0,0);glUniform1f(glGetUniformLocation(program,"vrHudSize"),1);glUniform1f(glGetUniformLocation(program,"vrWorldZoom"),1);
    constexpr unsigned width=512,height=512;GLuint fbo{},texture{};glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Canvas raster framebuffer incomplete");
    glViewport(0,0,width,height);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    const auto pixel=[&](float x,float y){auto p=vp*glm::vec4((x/320-1)*overlay.x*overlay.z,(y/240-1)*overlay.y*overlay.z,-overlay.z,1);return glm::vec2((p.x/p.w*.5f+.5f)*width,(p.y/p.w*.5f+.5f)*height);};
    const auto a=pixel(0,0),b=pixel(640,480);const float left=std::min(a.x,b.x),right=std::max(a.x,b.x),bottom=std::min(a.y,b.y),top=std::max(a.y,b.y);
    require(left>4&&right<width-4&&bottom>4&&top<height-4,"Eye fixture does not expose canvas outside margins");
    std::vector<unsigned char> rgba(width*height*4);
    const auto draw=[&](float x0,float y0,float x1,float y1,float marker){
        constexpr float nativeZ=.49751242995262146f;const float positions[]{x0,y0,nativeZ,x0,y1,nativeZ,x1,y0,nativeZ,x1,y1,nativeZ};
        glBufferData(GL_ARRAY_BUFFER,sizeof positions,positions,GL_STREAM_DRAW);auto o=overlay;o.w=marker;glUniform4fv(glGetUniformLocation(program,"vrOverlay"),1,&o[0]);
        std::array<glm::vec3,4> nativeW;glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,4);glEndTransformFeedback();
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof nativeW,nativeW.data());glDisable(GL_RASTERIZER_DISCARD);
        for(const auto& value:nativeW)require(std::abs(value.z-1.f/nativeZ)<.00001f,"Canvas clip changed native fragment W depth");
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLE_STRIP,0,4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    };
    const auto outside_count=[&](){unsigned count=0;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        if((x+.5f<left-2||x+.5f>right+2||y+.5f<bottom-2||y+.5f>top+2)&&rgba[(y*width+x)*4])++count;
    }return count;};
    draw(-256,-240,896,720,silent_scope::dc_native_panel::FlatMarker);
    require(outside_count()==0,"Flat native overscan painted outside the 640x480 canvas");
    const auto center=pixel(320,240);require(rgba[(unsigned(center.y)*width+unsigned(center.x))*4]==255,"Canvas clipping removed the visible native center");
    // Native adjacent surfaces offset by exactly +/-640 must not leak beside
    // the physical panel; vertical neighbors also cover both Y clip branches.
    for(const auto& rect:std::array<std::array<float,4>,4>{{{{-640,0,0,480}},{{640,0,1280,480}},{{0,-480,640,0}},{{0,480,640,960}}}}){
        draw(rect[0],rect[1],rect[2],rect[3],silent_scope::dc_native_panel::FlatMarker);
        require(outside_count()==0,"Adjacent native surface escaped the physical canvas");
    }
    // Ordinary overlay and world geometry must initialize the new varying to
    // z=0: the exact same fragment clip must leave their overscan untouched.
    for(float marker:{1.f/.49751242995262146f,-1.f}){
        draw(-256,-240,896,720,marker);require(outside_count()>1000,"Canvas clip affected ordinary overlay/world geometry");
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fbo);glDeleteProgram(program);glDeleteShader(fs);
}

static void fade_raster(GLuint vs,GLuint vao,GLuint vbo,const std::string& gltext,glm::mat4 ndc,glm::vec2 tanHalf){
    const auto start=gltext.find("    if(vrNativeRaster.z>0.5");
    const auto stop=gltext.find(std::string(1,char(10))+"#endif",start);
    require(start!=std::string::npos&&stop!=std::string::npos,"Missing actual Fade fragment");
    const auto fragment=std::string(R"FADE(#version 330 core
uniform vec3 vrNativeRaster;
in vec4 vtx_nativeRaster;
out vec4 color;
void main(){
    color=vec4(.8,.6,.4,.37);
)FADE")+gltext.substr(start,stop-start)+"}";
    const auto fs=shader(GL_FRAGMENT_SHADER,fragment);
    GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);
    glBindAttribLocation(program,0,"in_pos");glLinkProgram(program);
    GLint good{};glGetProgramiv(program,GL_LINK_STATUS,&good);require(good,"Fade program failed to link");glUseProgram(program);
    constexpr unsigned size=512;
    GLuint fbo{},tex{};glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Fade framebuffer failed");
    glViewport(0,0,size,size);glDisable(GL_RASTERIZER_DISCARD);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    const auto vp=projection(tanHalf*3.f,.0001f);
    glUniformMatrix4fv(glGetUniformLocation(program,"ndcMat"),1,GL_FALSE,&ndc[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program,"vrMat"),1,GL_FALSE,&vp[0][0]);
    glUniform2f(glGetUniformLocation(program,"vrTan"),tanHalf.x*3,tanHalf.y*3);
    glUniform3f(glGetUniformLocation(program,"vrComfort"),0,0,0);
    glUniform1f(glGetUniformLocation(program,"vrHudSize"),1);glUniform1f(glGetUniformLocation(program,"vrWorldZoom"),1);
    const float positions[]{0,0,.1f,0,480,.1f,640,0,.1f,640,480,.1f};
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof positions,positions,GL_STREAM_DRAW);
    const auto draw=[&](bool fade,bool hud){
        glUniform3f(glGetUniformLocation(program,"vrNativeRaster"),tanHalf.x,tanHalf.y,fade?1:0);
        glUniform4f(glGetUniformLocation(program,"vrOverlay"),tanHalf.x*3,tanHalf.y*3,10,hud?10:-1);
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        std::array<unsigned char,4> inside{},outside{};
        glReadPixels(256,256,1,1,GL_RGBA,GL_UNSIGNED_BYTE,inside.data());
        glReadPixels(375,256,1,1,GL_RGBA,GL_UNSIGNED_BYTE,outside.data());
        require(inside[0]>=203,"Fade changed the native interior");
        require(std::abs(int(outside[3])-int(inside[3]))<=1,"Fade changed alpha");
        require(fade&&!hud?outside[0]<80:outside[0]>=203,"Fade boundary or HUD exemption failed");
    };
    draw(true,false);draw(false,false);draw(true,true);
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&tex);glDeleteProgram(program);glDeleteShader(fs);
}
int main(int argc,char**argv){try{
    require(argc==3,"Arguments: actual gles.cpp actual vr_reproject.cpp");
    auto gltext=read(argv[1]),vrtext=read(argv[2]);const std::string begin="static const char* VertexShaderSource = R\"(";
    auto at=gltext.find(begin);require(at!=std::string::npos,"Missing production shader");at+=begin.size();const auto end=gltext.find(")\";",at);const auto source=gltext.substr(at,end-at);
    require(source.find("if (vrOverlay.w == -2.0)")!=std::string::npos,"Native flat-panel shader path missing");
    const auto clipAt=gltext.find("// SS_NATIVE_CANVAS_CLIP_BEGIN"),clipEnd=gltext.find("// SS_NATIVE_CANVAS_CLIP_END",clipAt);
    require(clipAt!=std::string::npos&&clipEnd!=std::string::npos,"Production native-canvas clip markers missing");
    const auto clip=gltext.substr(clipAt,clipEnd-clipAt);require(clip.find("vtx_native_canvas")!=std::string::npos&&clip.find("discard")!=std::string::npos,"Production canvas clip block incomplete");
    const std::string inactive="params.viewProj = projection(params.tanHalf, zNear);";
    require(vrtext.find(inactive)!=std::string::npos && vrtext.find("silent_scope::dc_native_panel::apply")!=std::string::npos,"Native flat-panel source hook missing");
    silent_scope::GLContext context;
    const std::string prefix="#version 330 core\n#define TARGET_GL 3\n#define GLES2 2\n#define DIV_POS_Z 1\n#define INTERPOLATION\n#define VR_REPROJECT 1\n#define pp_Gouraud 1\n";
    const auto vs=shader(GL_VERTEX_SHADER,prefix+source),fs=shader(GL_FRAGMENT_SHADER,"#version 330 core\nout vec4 color;void main(){color=vec4(1);}");
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glBindAttribLocation(p,0,"in_pos");
    const char* varying="gl_Position";glTransformFeedbackVaryings(p,1,&varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);GLint ok{};glGetProgramiv(p,GL_LINK_STATUS,&ok);require(ok,"Program link failed");glUseProgram(p);
    // Actual source dc-ui-warning-001 native10.21743072, first foreground quad.
    // Material TR/c8800000/949004ed/828c0002; primary actual vertices, not a mock menu.
    constexpr float positions[]{38,112,.49751242995262146f,38,296,.49751242995262146f,590,112,.49751242995262146f};
    GLuint vao{},vbo{},feedback{};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof positions,positions,GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,12,nullptr);glEnableVertexAttribArray(0);
    glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,3*sizeof(glm::vec4),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
    glm::mat4 ndc(1);ndc[0][0]=1.f/320;ndc[1][1]=1.f/240;ndc[3][0]=-1;ndc[3][1]=-1;
    const glm::vec2 tanHalf(320.f/555,240.f/555);const auto inactiveMatrix=projection(tanHalf,.00001f);
    glUniformMatrix4fv(glGetUniformLocation(p,"ndcMat"),1,GL_FALSE,&ndc[0][0]);glUniformMatrix4fv(glGetUniformLocation(p,"vrMat"),1,GL_FALSE,&inactiveMatrix[0][0]);
    glUniform2fv(glGetUniformLocation(p,"vrTan"),1,&tanHalf[0]);glUniform4f(glGetUniformLocation(p,"vrOverlay"),tanHalf.x,tanHalf.y,1,-1);glUniform3f(glGetUniformLocation(p,"vrComfort"),0,0,0);glUniform1f(glGetUniformLocation(p,"vrHudSize"),1);glUniform1f(glGetUniformLocation(p,"vrWorldZoom"),1);

    struct Params{glm::vec2 tanHalf;glm::vec4 overlay;glm::mat4 viewProj;};
    const Params legacy{tanHalf,glm::vec4(tanHalf,1,-1),inactiveMatrix};
    for(unsigned gate=0;gate<7;++gate){auto q=legacy;
        require(!silent_scope::dc_native_panel::apply(gate==0?"OTHER":"T9507N",gate==1,gate==2?320:640,480,gate!=3,gate!=4,gate==5?0.f:.1f,1.8f,gate==6?glm::mat4(std::numeric_limits<float>::quiet_NaN()):glm::mat4(1),q),"Panel presentation gate bypassed");
        require(q.overlay==legacy.overlay,"Rejected panel changed params");}
    glEnable(GL_RASTERIZER_DISCARD);std::array<std::array<glm::vec4,3>,2> result;
    const glm::mat4 eyes[]{eye(std::tan(glm::radians(-50.f)),std::tan(glm::radians(40.f))),eye(std::tan(glm::radians(-40.f)),std::tan(glm::radians(50.f)))};
    const float eyeX[]{-.032f,.032f};
    for(float distance:{1.8f,2.5f}){
        for(unsigned i=0;i<2;++i){auto q=legacy;const auto vp=eyes[i]*glm::translate(glm::mat4(1),glm::vec3(-eyeX[i],0,0))*glm::scale(glm::mat4(1),glm::vec3(.1f,-.1f,.1f));
            require(silent_scope::dc_native_panel::apply("T9507N",false,640,480,true,true,.1f,distance,vp,q),"Native panel setup failed");
            glUniformMatrix4fv(glGetUniformLocation(p,"vrMat"),1,GL_FALSE,&q.viewProj[0][0]);glUniform4fv(glGetUniformLocation(p,"vrOverlay"),1,&q.overlay[0]);
            glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,3);glEndTransformFeedback();glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof result[i],result[i].data());}
        const auto top=result[0][0]/result[0][0].w,bottom=result[0][1]/result[0][1].w;
        const float lx=eyeX[0]+(top.x+eyes[0][2][0])/eyes[0][0][0]*distance;
        const float rx=eyeX[1]+(result[1][0].x/result[1][0].w+eyes[1][2][0])/eyes[1][0][0]*distance;
        require(top.y>bottom.y,"Native menu remains inverted");require(std::abs(lx-rx)<.00001f,"Native menu eye rays do not converge");
        require(std::abs(result[0][0].w-distance)<.00001f,"Saved physical panel distance ignored");
        require(std::abs(lx-(38.f/320-1)*tanHalf.x*distance)<.00001f,"Panel mapping disagrees with native flat picker");
    }
    // Same production modifier fragment and vertex source as create_modvol_shader.
    const std::string marker="static const char* ModifierVolumeShader = R\"(";auto modAt=gltext.find(marker);require(modAt!=std::string::npos,"Missing production modifier shader");modAt+=marker.size();const auto modEnd=gltext.find(")\";",modAt);
    const auto mod=shader(GL_FRAGMENT_SHADER,"#version 330 core\n#define TARGET_GL 3\n#define GLES2 2\n#define DIV_POS_Z 1\n#define gl_FragColor color\nout vec4 color;\n"+gltext.substr(modAt,modEnd-modAt));
    GLuint modifier=glCreateProgram();glAttachShader(modifier,vs);glAttachShader(modifier,mod);glBindAttribLocation(modifier,0,"in_pos");glTransformFeedbackVaryings(modifier,1,&varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(modifier);glGetProgramiv(modifier,GL_LINK_STATUS,&ok);require(ok,"Actual modifier program failed to link");
    glUseProgram(modifier);auto q=legacy;const auto vp=eyes[1]*glm::translate(glm::mat4(1),glm::vec3(-eyeX[1],0,0))*glm::scale(glm::mat4(1),glm::vec3(.1f,-.1f,.1f));require(silent_scope::dc_native_panel::apply("T9507N",false,640,480,true,true,.1f,2.5f,vp,q),"Modifier panel setup failed");
    glUniformMatrix4fv(glGetUniformLocation(modifier,"ndcMat"),1,GL_FALSE,&ndc[0][0]);glUniformMatrix4fv(glGetUniformLocation(modifier,"vrMat"),1,GL_FALSE,&q.viewProj[0][0]);glUniform4fv(glGetUniformLocation(modifier,"vrOverlay"),1,&q.overlay[0]);
    std::array<glm::vec4,3> modResult;glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,3);glEndTransformFeedback();glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof modResult,modResult.data());
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<4;++j)require(std::abs(modResult[i][j]-result[1][i][j])<.00001f,"Modifier/color panel placement disagrees");
    for(unsigned i=0;i<2;++i){auto raster=legacy;const auto viewProjection=eyes[i]*glm::translate(glm::mat4(1),glm::vec3(-eyeX[i],0,0))*glm::scale(glm::mat4(1),glm::vec3(.1f,-.1f,.1f));
        require(silent_scope::dc_native_panel::apply("T9507N",false,640,480,true,true,.1f,1.8f,viewProjection,raster),"Raster canvas panel setup failed");
        canvas_raster(vs,vao,vbo,clip,ndc,raster.viewProj,raster.overlay,tanHalf);}
    fade_raster(vs,vao,vbo,gltext,ndc,tanHalf);
    require(glGetError()==GL_NO_ERROR,"Unexpected GL error");std::cout<<"Native menu GL: actual warning upright; asymmetric stereo convergence;1.8/2.5m; matching picker; modifier/color parity; native overscan/neighbors clipped in both eyes; ordinary overlay/world preserved; RTT/desktop/other-title gates passed\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
