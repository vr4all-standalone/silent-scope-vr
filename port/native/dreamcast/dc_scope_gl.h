// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Include the owning renderer's GL loader before this header. These adapters
// use that loader, including Flycast's libretro GLSM aliases when selected.
#include "dc_scope.h"
namespace silent_scope::dc_scope {
namespace gl_detail {
struct Guard {
    GLint read{},draw{},read_buffer{},active{},texture{},unpack{};
    GLboolean scissor{};
    Guard() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_READ_BUFFER,&read_buffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);
        scissor=glIsEnabled(GL_SCISSOR_TEST);
    }
    ~Guard() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(read));glReadBuffer(GLenum(read_buffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,GLuint(draw));
        glActiveTexture(GLenum(active));glBindTexture(GL_TEXTURE_2D,GLuint(texture));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,GLuint(unpack));
        if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
    }
};
inline void destroy(Target target) {
    if(target.framebuffer){const GLuint fbo=target.framebuffer;glDeleteFramebuffers(1,&fbo);}
    if(target.texture){const GLuint texture=target.texture;glDeleteTextures(1,&texture);}
}
inline Target create() {
    Guard guard;Target target;
    GLuint texture{},framebuffer{};
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    const GLenum attachment=GL_COLOR_ATTACHMENT0;glDrawBuffers(1,&attachment);
    const bool complete=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    const GLenum error=glGetError();target={texture,framebuffer};
    if(!texture||!framebuffer||!complete||error!=GL_NO_ERROR){destroy(target);return {};}
    return target;
}
inline CopyResult copy(Target target,unsigned source_fbo) {
    Guard guard;
    if(!source_fbo || source_fbo==target.framebuffer || guard.read!=GLint(source_fbo) || guard.draw!=GLint(source_fbo) ||
       glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return {};
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,target.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0,0,256,256,0,0,256,256,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    const GLenum error=glGetError();
    return {error==GL_NO_ERROR,error};
}
}
inline GlApi current_gl_api() { return {gl_detail::create,gl_detail::copy,gl_detail::destroy}; }
}
