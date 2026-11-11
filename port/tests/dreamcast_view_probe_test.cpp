#include <iostream>
#if __has_include("dreamcast/native_view_probe.h")
#include "dreamcast/native_view_probe.h"
#include "gl_context.h"
#include <glad/gl.h>
#define HAS_DC_PROBE 1
#endif
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;}}while(false)
#ifdef HAS_DC_PROBE
using namespace silent_scope::dc_probe;
struct Vertex{float x{},y{},z{};};struct Poly{unsigned first{},count{};};
struct Context {
    unsigned framebufferWidth{2},framebufferHeight{2},fb_W_SOF1{0x12340},fb_W_LINESTRIDE{1};
    struct {unsigned fb_packmode{1};} fb_W_CTRL;
    struct {struct XY{int x{},y{};} origin,size{2,2};}fbClip;
    std::vector<Vertex> verts{{0,0,.5f},{1,0,1},{0,1,.5f},{1,1,2}};
    std::vector<unsigned> idx{0,1,2,3,3,0xffffffff};std::vector<Poly> global_param_op{{0,6}},global_param_pt,global_param_tr;
    std::vector<int> modtrig;
};
int integer(unsigned name){GLint result{};glGetIntegerv(name,&result);return result;}
GlApi api(){return {integer,[](unsigned name,int value){glPixelStorei(name,value);},
    [](unsigned target,unsigned value){glBindBuffer(target,value);},
    []{return glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;},
    [](unsigned w,unsigned h,void* data){glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,data);}};}
#endif
int main(){
#ifndef HAS_DC_PROBE
    REQUIRE(false && "Bounded native Dreamcast view probe not implemented");
#else
    const auto directory=std::filesystem::absolute("private")/("dc-view-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(!std::filesystem::exists(directory));std::filesystem::create_directories(directory);
    NativeViewProbe absent(directory/"not-created");REQUIRE(!absent.enabled_for("T9507N") && !std::filesystem::exists(directory/"not-created"));
    auto metadata=describe_context(Context{},PassKind::Rtt);REQUIRE(metadata.geometry.vertices==4 && metadata.geometry.indices==6);
    REQUIRE(metadata.geometry.context_indexed_triangles==2 && metadata.geometry.q_min==.5 && metadata.geometry.q_max==2);
    REQUIRE(metadata.destination==0x12340 && metadata.stride==8 && metadata.clip_width==2);

    silent_scope::GLContext context;GLuint framebuffer{},texture{},other{},pack_buffer{},bound_texture{};
    glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    const unsigned char rgba[]{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glGenFramebuffers(1,&other);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,other);
    glGenBuffers(1,&pack_buffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,pack_buffer);std::vector<unsigned char> sentinel(8192,0xab);
    glBufferData(GL_PIXEL_PACK_BUFFER,sentinel.size(),sentinel.data(),GL_STATIC_DRAW);
    glPixelStorei(GL_PACK_ALIGNMENT,8);glPixelStorei(GL_PACK_ROW_LENGTH,8);glPixelStorei(GL_PACK_IMAGE_HEIGHT,7);
    glPixelStorei(GL_PACK_SKIP_ROWS,2);glPixelStorei(GL_PACK_SKIP_PIXELS,1);glPixelStorei(GL_PACK_SKIP_IMAGES,1);
    glPixelStorei(GL_PACK_SWAP_BYTES,1);glPixelStorei(GL_PACK_LSB_FIRST,1);
    glGenTextures(1,&bound_texture);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,bound_texture);
    const unsigned fields[]{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_IMAGE_HEIGHT,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_IMAGES,
        GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST,GL_PIXEL_PACK_BUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_BUFFER,GL_ACTIVE_TEXTURE,GL_TEXTURE_BINDING_2D};
    std::vector<int> before;for(auto field:fields)before.push_back(integer(field));
    ProbeLimits limits;limits.metadata_interval_seconds=0;limits.image_interval_seconds=0;
    limits.max_images=100;limits.max_records=1000;limits.max_dimension=4096;limits.max_bytes=1024ull*1024*1024;
    NativeViewProbe probe(directory,limits);REQUIRE(!probe.enabled_for("T9505D") && probe.enabled_for("T9507N"));
    REQUIRE(!probe.record("NOT-SILENT-SCOPE",metadata,api(),1).recorded);
    glEnable(0xdead);auto result=probe.record("T9507N",metadata,api(),1);REQUIRE(result.recorded && result.captured);
    REQUIRE(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR);
    for(std::size_t i=0;i<before.size();++i)REQUIRE(integer(fields[i])==before[i]);
    std::vector<unsigned char> actual_pack(sentinel.size());glGetBufferSubData(GL_PIXEL_PACK_BUFFER,0,actual_pack.size(),actual_pack.data());REQUIRE(actual_pack==sentinel);
    std::ifstream image(result.image_path,std::ios::binary);std::vector<unsigned char> captured((std::istreambuf_iterator<char>(image)),{});
    REQUIRE(captured==std::vector<unsigned char>(std::begin(rgba),std::end(rgba)));
    auto failing=api();failing.read_rgba=[](unsigned,unsigned,void*){throw std::runtime_error("diagnostic failure");};
    result=probe.record("T9507N",metadata,failing,2);REQUIRE(result.recorded && !result.captured);
    for(std::size_t i=0;i<before.size();++i)REQUIRE(integer(fields[i])==before[i]);
    auto too_large=metadata;too_large.width=2049;result=probe.record("T9507N",too_large,api(),3);REQUIRE(result.recorded && !result.captured);
    auto main=metadata;main.kind=PassKind::Main;REQUIRE(probe.record("T9507N",main,api(),4).captured);
    for(unsigned i=0;i<320;++i){metadata.kind=i%2?PassKind::Main:PassKind::Rtt;probe.record("T9507N",metadata,api(),5+i*.1);}
    // Failed readbacks also consume the hard image-work budget.
    REQUIRE(probe.stats().metadata_records==256 && probe.stats().readback_attempts==32 && probe.stats().images==31 && probe.stats().bytes==31*16);
    REQUIRE(!probe.record("T9507N",metadata,api(),37).recorded);
    std::ifstream json(probe.metadata_path());std::string line;unsigned count=0;bool orientation=false,unlabeled=false,failure=false;
    while(std::getline(json,line)){++count;orientation|=line.find("lower-left")!=std::string::npos;unlabeled|=line.find("\"kind\":\"main\"")!=std::string::npos;failure|=line.find("readback_exception")!=std::string::npos;}
    REQUIRE(count==256 && orientation && unlabeled && failure);
    NativeViewProbe elapsed(directory,limits);REQUIRE(elapsed.record("T9507N",metadata,api(),10).recorded);
    REQUIRE(!elapsed.record("T9507N",metadata,api(),131).recorded);
    ProbeLimits tiny=limits;tiny.max_bytes=15;NativeViewProbe byte_limit(directory,tiny);
    REQUIRE(!byte_limit.record("T9507N",metadata,api(),1).captured && byte_limit.stats().bytes==0);
    ProbeLimits late=limits;late.min_image_elapsed=40;NativeViewProbe delayed(directory,late);
    REQUIRE(delayed.record("T9507N",metadata,api(),10).recorded);
    REQUIRE(delayed.record("T9507N",metadata,api(),49.99).recorded);
    REQUIRE(delayed.stats().readback_attempts==0 && delayed.stats().images==0);
    REQUIRE(delayed.record("T9507N",metadata,api(),50).captured);
    REQUIRE(delayed.stats().readback_attempts==1 && delayed.stats().images==1);
    ProbeLimits timed;NativeViewProbe sampled(directory,timed);
    REQUIRE(sampled.record("T9507N",metadata,api(),1).captured);
    REQUIRE(!sampled.record("T9507N",metadata,api(),1.1).recorded);
    auto sampled_result=sampled.record("T9507N",metadata,api(),2.1);REQUIRE(sampled_result.recorded && !sampled_result.captured);
    struct Comma:std::numpunct<char>{char do_decimal_point()const override{return ',';}};
    const auto previous=std::locale();std::locale::global(std::locale(previous,new Comma));
    NativeViewProbe locale_probe(directory,limits);REQUIRE(locale_probe.record("T9507N",metadata,api(),1.25).recorded);
    std::locale::global(previous);std::ifstream locale_json(locale_probe.metadata_path());std::string locale_line;std::getline(locale_json,locale_line);
    REQUIRE(locale_line.find("\"host_monotonic_seconds\":1.25")!=std::string::npos && locale_line.find("\"q_min\":0.5")!=std::string::npos);
    REQUIRE(glGetError()==GL_NO_ERROR);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glDeleteBuffers(1,&pack_buffer);glDeleteTextures(1,&texture);glDeleteTextures(1,&bound_texture);glDeleteFramebuffers(1,&other);glDeleteFramebuffers(1,&framebuffer);
    std::cout<<"Private synthetic GPU probe evidence: "<<directory.string()<<'\n';
#endif
    std::cout<<"Dreamcast probe: state/PBO preservation, RGBA origin, native context counts and hard budgets passed\n";
}
