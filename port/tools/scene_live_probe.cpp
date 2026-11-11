// SPDX-License-Identifier: GPL-2.0-or-later
#include "scene/gl_scene.h"
#include "scene/scene_geometry.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
using namespace silent_scope::scene;
template<class T>T value(std::istream& in){T result{};in.read(reinterpret_cast<char*>(&result),sizeof(result));if(!in)throw std::runtime_error("Truncated scene trace");return result;}
std::shared_ptr<RawFrame> read(std::istream& in,double wider_factor){
    char magic[8];in.read(magic,8);if(std::string(magic,8)!="CVTRC005")throw std::runtime_error("Expected CVTRC005");
    auto f=std::make_shared<RawFrame>();f->sequence=value<uint64_t>(in);f->width=value<uint32_t>(in);f->height=value<uint32_t>(in);f->clear_color=value<uint32_t>(in);f->x_offset=value<int32_t>(in);f->y_offset=value<int32_t>(in);
    f->complete=value<uint32_t>(in);auto triangles=value<uint32_t>(in),textures=value<uint32_t>(in);
    if(f->width!=512||f->height!=384||triangles>32768||textures>triangles*2)throw std::runtime_error("Unexpected trace dimensions/counts");
    auto count=value<uint32_t>(in);if(count&&count!=f->width*f->height)throw std::runtime_error("Invalid background");
    f->background.resize(count);in.read(reinterpret_cast<char*>(f->background.data()),count*4);
    count=value<uint32_t>(in);if(count&&count!=f->width*f->height)throw std::runtime_error("Invalid background depth");
    f->background_depth.resize(count);in.read(reinterpret_cast<char*>(f->background_depth.data()),count*2);
    std::vector<std::array<uint32_t,2>> refs;f->triangles.resize(triangles);
    for(auto& t:f->triangles){
        for(auto& v:t.vertex){v.x=value<double>(in);v.y=value<double>(in);v.z=value<double>(in);v.q=value<double>(in);for(double* p:{&v.s[0],&v.s[1],&v.t[0],&v.t[1],&v.tq[0],&v.tq[1],&v.r,&v.g,&v.b,&v.a})*p=value<double>(in);}
        auto& s=t.state;for(uint32_t* p:{&s.color_path,&s.alpha_mode,&s.depth_mode,&s.fog_mode,&s.texture_mode,&s.color0,&s.color1,&s.fog_color,&s.chroma_key,&t.texture_mode1,&t.alpha_reference,&t.depth_constant,&t.clip_left,&t.clip_right,&t.clip_top,&t.clip_bottom})*p=value<uint32_t>(in);
        refs.push_back({value<uint32_t>(in),value<uint32_t>(in)});for(auto& fog:t.fog_table)fog=value<uint16_t>(in);t.fog_delta_mask=value<uint32_t>(in);t.overlay=value<uint32_t>(in);
    }
    std::vector<std::shared_ptr<const TextureImage>> images;uint64_t allocated{};
    for(uint32_t i=0;i<textures;++i){auto id=value<uint64_t>(in),generation=value<uint64_t>(in);auto w=value<uint32_t>(in),h=value<uint32_t>(in),lod=value<uint32_t>(in);allocated+=uint64_t(w)*h*4;if(!w||!h||w>4096||h>4096||allocated>64*1024*1024)throw std::runtime_error("Texture budget exceeded");std::vector<uint32_t> pixels(size_t(w)*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);images.push_back(std::make_shared<const TextureImage>(id,generation,w,h,std::move(pixels),lod));}
    if(!in)throw std::runtime_error("Truncated texture data");
    for(size_t i=0;i<refs.size();++i)for(int unit=0;unit<2;++unit)if(refs[i][unit]!=UINT32_MAX){if(refs[i][unit]>=images.size())throw std::runtime_error("Invalid texture reference");f->triangles[i].texture[unit]=images[refs[i][unit]];}
    // CVTRC005 contains no camera metadata. This explicitly supplied diagnostic
    // lens must come from the matching native probe tuple/sidecar, never guessed
    // from requested settings. Do not mark it as source-owned capture metadata.
    f->lens={512,384,443/wider_factor,443/wider_factor,256,192};return f;
}
int main(int argc,char** argv){try{
    if(argc<3||argc>5){std::cerr<<"scene_live_probe TRACE OUTPUT_PREFIX [BENCHMARK_FRAMES] [WIDER_FACTOR]\n";return 1;}
    int benchmark=argc>=4?std::stoi(argv[3]):0;if(benchmark<0||benchmark>1000)throw std::runtime_error("Invalid benchmark duration");
    double wider_factor=1;
    if(argc==5){char* end=nullptr;wider_factor=std::strtod(argv[4],&end);
        if(end==argv[4]||*end||!std::isfinite(wider_factor)||wider_factor<1||wider_factor>4)throw std::runtime_error("Invalid external Wider factor: expected finite1..4");}
    std::ifstream input(argv[1],std::ios::binary);auto frame=read(input,wider_factor);
    std::cout<<"CVTRC005 external_lens_factor="<<wider_factor<<" fx="<<frame->lens.fx<<" fy="<<frame->lens.fy<<" (camera metadata is not stored in this trace format)\n";
    std::vector<double> depths;for(const auto& tri:frame->triangles)if(!tri.overlay)for(const auto& v:tri.vertex)if(v.q>0&&std::isfinite(v.q))depths.push_back(1/v.q);
    if(depths.empty())throw std::runtime_error("No spatial geometry in trace");
    std::sort(depths.begin(),depths.end());std::cout<<"triangles="<<frame->triangles.size()<<" depths p10/p50/p90="<<depths[depths.size()/10]<<'/'<<depths[depths.size()/2]<<'/'<<depths[depths.size()*9/10]<<'\n';
    silent_scope::GLContext context;GLScene scene;GLuint target{},color{},depth{};glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,384,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,384);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(benchmark){glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1536,1536,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,1536,1536);glViewport(0,0,1536,1536);}
    scene.prepare(frame,false);
    if(benchmark){
        using Clock=std::chrono::steady_clock;double submit{},complete{};auto projection=glm::perspective(glm::radians(90.f),1.f,.005f,100000.f);
        for(int i=-8;i<benchmark;i++){auto start=Clock::now();for(float eye:{-.032f,.032f})scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),SceneMetresPerUnit,1.8f);auto submitted=Clock::now();glFinish();auto finished=Clock::now();if(i>=0){submit+=std::chrono::duration<double,std::milli>(submitted-start).count();complete+=std::chrono::duration<double,std::milli>(finished-start).count();}}
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU benchmark error");
        std::cout<<"renderer="<<glGetString(GL_RENDERER)<<" stereo_submit_ms="<<submit/benchmark<<" stereo_complete_ms="<<complete/benchmark<<'\n';return 0;
    }
    for(auto [name,eye]:{std::pair{"center",0.f},std::pair{"left",-.032f},std::pair{"right",.032f}}){
        glViewport(0,0,512,384);auto projection=glm::frustum(float(-256/frame->lens.fx*.005),float(256/frame->lens.fx*.005),float(-192/frame->lens.fy*.005),float(192/frame->lens.fy*.005),.005f,100000.f);scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),SceneMetresPerUnit,1.8f);
        // RGBA/UNSIGNED_BYTE is guaranteed for this GLES RGBA8 target; RGB
        // readback is not supported by the Quest driver.
        std::vector<unsigned char> pixels(512*384*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,512,384,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(std::string(argv[2])+"-"+name+".ppm",std::ios::binary);out<<"P6\n512 384\n255\n";for(int y=383;y>=0;--y)for(int x=0;x<512;++x)out.write(reinterpret_cast<const char*>(pixels.data()+(y*512+x)*4),3);
    }
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU error");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
