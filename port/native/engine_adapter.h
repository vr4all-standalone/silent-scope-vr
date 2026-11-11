// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shared_runtime.h"
#include "host_poll.h"
#include "modules/lib/osdlib.h"
#include "scene/voodoo_hooks.h"
#include "scene/game_camera.h"
#include "scene/frame_composition.h"
#include "scene/trace_file.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string_view>
#ifdef __ANDROID__
#include <jni.h>
extern "C" void* SDL_AndroidGetJNIEnv(void);
extern "C" void* SDL_AndroidGetActivity(void);
#endif

namespace silent_scope {
inline bool vr_requested(){return std::getenv("SILENT_SCOPE_VR")!=nullptr;}
struct EngineHost {
    osd::dynamic_module::ptr module;
    void(*stop)(){};
    EngineHost(){
        const char* library=std::getenv("SILENT_SCOPE_XR_LIBRARY");
#ifdef _WIN32
        module=osd::dynamic_module::open({library?library:"libsilent_scope_xr.dll"});
#else
        module=osd::dynamic_module::open({library?library:"libsilent_scope_xr.so"});
#endif
        auto start=module->bind<bool(*)(SharedRuntime*)>("silent_scope_xr_start");
        stop=module->bind<void(*)()>("silent_scope_xr_stop");
#ifdef __ANDROID__
        auto init=module->bind<void(*)(void*,void*)>("silent_scope_xr_android_init");
        auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());JavaVM* vm{};
        if(env&&init){env->GetJavaVM(&vm);auto activity=SDL_AndroidGetActivity();init(vm,activity);env->DeleteLocalRef(static_cast<jobject>(activity));}
#endif
        if(!start||!stop||!start(&shared_runtime()))shared_runtime().quit=true;
    }
    ~EngineHost(){if(stop)stop();}
};
inline std::unique_ptr<EngineHost>& engine_host(){static std::unique_ptr<EngineHost> host;return host;}
inline std::array<uint64_t,2>& display_sequences(){static std::array<uint64_t,2> sequences{};return sequences;}
inline void reset_engine(){shared_runtime().reset();display_sequences()={};scene::voodoo_capture().reset();scene::invalidate_captured_textures();}
inline void stop_engine(){engine_host().reset();reset_engine();}
template<class Machine> void update_cabinet(Machine& machine) {
    auto& runtime=shared_runtime();
    if(runtime.quit){machine.schedule_exit();return;}
    CabinetInput input;bool start,coin,service;
    {std::lock_guard lock(runtime.input_mutex);input=runtime.input;start=runtime.start;coin=runtime.coin;service=runtime.service;}
    auto set=[&](const char* tag,uint32_t mask,uint32_t value){
        if(auto* port=machine.root_device().ioport(tag))if(auto* field=port->field(mask))field->set_value(value);
    };
    set(":ANALOG1",0x7ff,input.yaw);set(":ANALOG2",0x7ff,input.pitch);
    set(":IN0",0x04,input.trigger);set(":IN0",0x80,start);
    set(":IN2",0x80,coin);set(":IN2",0x10,service);
}
template<unsigned Which,class Machine,class Screen,class Bitmap>
void publish_screen(Machine& machine,Screen& screen,const Bitmap& bitmap,scene::NativeLens lens={},const Bitmap* overlay=nullptr) {
    if((!vr_requested()&&!scene::capture_enabled())||std::string_view(machine.system().name)!="sscope")return;
    if(vr_requested()&&!engine_host())engine_host()=std::make_unique<EngineHost>();
    if constexpr(Which==0) {
        if(vr_requested()&&!std::getenv("SILENT_SCOPE_SIMULATION"))poll_host(machine);
        else if(shared_runtime().quit)machine.schedule_exit();
    }
    const auto visible=screen.visible_area();
    auto width=visible.width(),height=visible.height();
    if(width<=0||height<=0||width>2048||height>2048)return;
    auto& runtime=shared_runtime();
    RasterFrame frame{runtime.displays.generation(),++display_sequences()[Which],machine.time().as_double(),
                      uint32_t(width),uint32_t(height),4.0/3.0,{}};
    frame.pixels.resize(size_t(width)*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        frame.pixels[size_t(y)*width+x]=bitmap.pix(y+visible.min_y,x+visible.min_x);
    if constexpr(Which==0){
        std::vector<uint32_t> tiles;
        if(overlay){tiles.resize(size_t(width)*height);for(int y=0;y<height;++y)for(int x=0;x<width;++x)tiles[size_t(y)*width+x]=overlay->pix(y+visible.min_y,x+visible.min_x);}
        auto spatial=scene::compose_frame(scene::voodoo_capture().acquire(),lens,width,height,{},std::move(tiles));
        runtime.main_scene.store(spatial);
        runtime.wider_applied=spatial&&spatial->wider_applied;
        static auto began=std::chrono::steady_clock::now();static double native_began=machine.time().as_double();static uint64_t previous_video{},previous_scene{};
        auto now=std::chrono::steady_clock::now();double elapsed=std::chrono::duration<double>(now-began).count();
        scene::configure_emulation_profile(std::getenv("SILENT_SCOPE_CPU_PROFILE"),machine.time().as_double());
        if(elapsed>=5){
            auto swaps=spatial?spatial->scene_sequence:0;
            runtime.native_scene_hz=(swaps>=previous_scene?(swaps-previous_scene)/elapsed:0);
            if(const char* path=std::getenv("SILENT_SCOPE_XR_LOG")){
                std::ofstream out(std::string(path)+".engine",std::ios::app);
                out<<"native_seconds="<<machine.time().as_double()<<" speed_percent="<<(machine.time().as_double()-native_began)/elapsed*100
                   <<" video_hz="<<(frame.sequence-previous_video)/elapsed<<" scene_hz="<<(swaps>=previous_scene?(swaps-previous_scene)/elapsed:0)
                   <<" triangles="<<(spatial?spatial->triangles.size():0)<<" camera="<<(spatial&&spatial->lens.valid())<<'\n';
                if(scene::emulation_profile().enabled){for(auto [tag,ns]:scene::emulation_profile().nanoseconds)out<<tag<<'='<<100.*ns/(elapsed*1e9)<<"% ";out<<'\n';scene::emulation_profile().nanoseconds.clear();}
            }
            previous_scene=swaps;
            previous_video=frame.sequence;began=now;native_began=machine.time().as_double();
        }
        if(const char* trace=std::getenv("SILENT_SCOPE_TRACE_DIR")){
            static int previous_second=-1,count=0;int second=int(machine.time().as_double());
            if(spatial&&second>=105&&second!=previous_second&&count<3){
                previous_second=second;++count;std::ofstream out(std::string(trace)+"/main-"+std::to_string(second)+".cvtrace",std::ios::binary);scene::write_trace(out,*spatial);
                std::ofstream image(std::string(trace)+"/native-"+std::to_string(second)+".ppm",std::ios::binary);image<<"P6\n"<<width<<' '<<height<<"\n255\n";
                for(auto color:frame.pixels){char rgb[3]={char(color>>16),char(color>>8),char(color)};image.write(rgb,3);}
            }
        }
    }
    runtime.displays.publish(Which==0?Display::Main:Display::Scope,std::move(frame));
}
}
