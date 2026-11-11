#include "gl_context.h"
#include <glad/gl.h>
#include "dreamcast/dc_scope_gl.h"
#include "dreamcast/dc_scope_composite.h"
#include <array>
#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
using namespace silent_scope::dc_scope;
int main() {
    silent_scope::GLContext context;
    GLuint source_fbo{},source_texture{};
    glGenFramebuffers(1,&source_fbo);glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);
    glGenTextures(1,&source_texture);glBindTexture(GL_TEXTURE_2D,source_texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB565,640,480,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,source_texture,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glDisable(GL_SCISSOR_TEST);glClearColor(1,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    const auto fill=[](int x,int y,float r,float g,float b){glScissor(x,y,128,128);glClearColor(r,g,b,1);glClear(GL_COLOR_BUFFER_BIT);};
    fill(0,0,1,0,0);fill(128,0,0,1,0);fill(0,128,0,0,1);fill(128,128,1,1,0);
    GLuint sentinel_texture{},unpack{};
    glGenTextures(1,&sentinel_texture);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,sentinel_texture);
    glGenBuffers(1,&unpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);glBufferData(GL_PIXEL_UNPACK_BUFFER,8,nullptr,GL_STATIC_DRAW);
    glScissor(3,4,17,23);glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);
    const GLenum fields[]{GL_ACTIVE_TEXTURE,GL_TEXTURE_BINDING_2D,GL_PIXEL_UNPACK_BUFFER_BINDING,
        GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_BUFFER};
    std::array<GLint,6> before{};for(size_t i=0;i<before.size();++i)glGetIntegerv(fields[i],&before[i]);
    ScopeGpu owner;const auto identity=reinterpret_cast<uintptr_t>(context.context());
    assert(owner.initialize(current_gl_api(),identity));owner.set_physical_rifle_active(true);
    Source source{"T9507N",true,true,640,480,0x52c000,1,512,0,0,256,256,10,3,source_fbo};
    assert(owner.capture(source,identity,10));
    for(size_t i=0;i<before.size();++i){GLint value{};glGetIntegerv(fields[i],&value);assert(value==before[i]);}
    assert(glIsEnabled(GL_SCISSOR_TEST));
    GLint scissor[4]{};glGetIntegerv(GL_SCISSOR_BOX,scissor);assert(scissor[0]==3&&scissor[1]==4&&scissor[2]==17&&scissor[3]==23);
    GLboolean mask[4]{};glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(!mask[0]&&mask[1]&&!mask[2]&&mask[3]);
    auto view=owner.acquire(11,3,identity,10.02);assert(view);
    GLuint read_fbo{};glGenFramebuffers(1,&read_fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,view.texture,0);
    assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    const auto pixel=[](int x,int y){std::array<unsigned char,4> p{};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
    const auto bl=pixel(32,32),br=pixel(224,32),tl=pixel(32,224),tr=pixel(224,224);
    assert(bl[0]>250&&bl[1]<5&&bl[2]<5&&bl[3]==255);
    assert(br[0]<5&&br[1]>250&&br[2]<5&&br[3]==255);
    assert(tl[0]<5&&tl[1]<5&&tl[2]>250&&tl[3]==255);
    assert(tr[0]>250&&tr[1]>250&&tr[2]<5&&tr[3]==255);
    // Later native source overwrite must not change the independently owned copy.
    glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);assert(pixel(32,32)==bl);
    // Copy RTT(next), then sample retained main(current)'s separate old texture.
    glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);source.submit_sequence=12;
    assert(owner.capture(source,identity,10.025));
    auto previous=owner.acquire(11,3,identity,10.03),next=owner.acquire(13,3,identity,10.03);
    assert(previous.texture==view.texture&&next.texture!=view.texture&&next.content_top_at_v0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);
    assert(pixel(32,32)==bl);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,next.texture,0);
    const auto black=pixel(32,32);assert(black[0]==0&&black[1]==0&&black[2]==0);
    // Real Quest ordering: two future RTTs can finish before retained main.
    // The third copy must not overwrite that main's independent old image.
    glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);
    source.submit_sequence=14;assert(owner.capture(source,identity,10.028));
    const auto queued=owner.acquire(15,3,identity,10.03);
    const auto displayed=owner.acquire(11,3,identity,10.03);
    assert(displayed&&displayed.submit_sequence==10&&queued&&queued.texture!=displayed.texture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,displayed.texture,0);
    assert(pixel(32,32)==bl);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,queued.texture,0);
    const auto green=pixel(32,32);assert(green[0]==0&&green[1]==255&&green[2]==0);
    // Real GPU image: freshness crossing during a stereo presentation cannot
    // give one eye a feed and the other a black optic. Next frame still expires.
    glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);source.submit_sequence=16;
    assert(owner.capture(source,identity,monotonic_seconds()-.24));
    {ScopedRifleDrawAuthorization presentation(true);
        const auto left=owner.acquire(17,3,identity,presentation_seconds());assert(left);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        const auto right=owner.acquire(17,3,identity,presentation_seconds());
        assert(right&&right.texture==left.texture&&right.sequence==left.sequence);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,right.texture,0);
        assert(pixel(32,32)==green);
    }
    {ScopedRifleDrawAuthorization next_frame(true);
        assert(!owner.acquire(17,3,identity,presentation_seconds()));
    }
    owner.invalidate();assert(!owner.acquire(11,3,identity,10.03));
    assert(!owner.acquire(13,3,identity,10.03));
    glBindFramebuffer(GL_FRAMEBUFFER,source_fbo);assert(owner.release(identity));
    assert(!glIsTexture(view.texture));assert(glIsTexture(source_texture));
    assert(!glIsTexture(next.texture));
    assert(!glIsTexture(queued.texture));
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&unpack);glDeleteTextures(1,&sentinel_texture);glDeleteTextures(1,&source_texture);
    glDeleteFramebuffers(1,&read_fbo);glDeleteFramebuffers(1,&source_fbo);
    std::cout<<"GPU scope: independent565copy,256crop,lowerleftorigin,GLstate,reset and ownership passed\n";
}
