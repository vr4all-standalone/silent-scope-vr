// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "voodoo_capture.h"
#include "native_camera.h"
#include "camera_command_queue.h"
#include "capture_texture_cache.h"

#include "emulation_profile.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace silent_scope::scene {
inline std::atomic<bool>& capture_flag() {
    static std::atomic<bool> flag{std::getenv("SILENT_SCOPE_CAPTURE") || std::getenv("SILENT_SCOPE_TRACE_DIR") || std::getenv("SILENT_SCOPE_VR")};
    return flag;
}
inline bool capture_enabled() { return capture_flag().load(std::memory_order_relaxed); }
inline bool capture_device(std::string_view tag) { return capture_enabled() && tag==":voodoo0"; }
inline void enable_capture(bool enabled) { capture_flag().store(enabled,std::memory_order_relaxed); }
inline std::unordered_map<const void*,std::shared_ptr<const TextureImage>>& texture_snapshots() {
    static std::unordered_map<const void*,std::shared_ptr<const TextureImage>> snapshots;
    return snapshots;
}
inline void invalidate_captured_textures() {
    if (capture_enabled()) texture_snapshots().clear();
}
template<class Texture>
std::shared_ptr<const TextureImage> capture_texture(Texture* texture,uint32_t mode) {
    if (!texture) return {};
    auto& snapshots=texture_snapshots();
    if (auto found=snapshots.find(texture);found!=snapshots.end()) return found->second;
    EmulationScope texture_profile("<texture-key>");
    static uint64_t generation{};
    auto& cache=captured_texture_cache();auto& source=cache.scratch();
    texture->capture_source((mode>>8)&15,source);
    auto image=cache.get(source,[&]{return std::make_shared<const TextureImage>(uint64_t(reinterpret_cast<uintptr_t>(texture)),
        ++generation,source.width,source.height,texture->capture_argb(source.format),source.lod);});

    snapshots[texture]=image;
    return image;
}
// Called on the emulation thread at native triangle command acceptance, not
// when delayed FIFO execution later reaches the rasterizer.
inline std::unordered_map<const void*,CameraCommandQueue>& command_camera_queues(){
    static std::unordered_map<const void*,CameraCommandQueue> queues;return queues;
}
inline void capture_accept_triangle(const void* device){
    if(!capture_enabled())return;
    auto camera=native_camera_state();
    if(camera&&!camera->lens.valid())report_native_camera_issue("command_accept_invalid");
    if(!command_camera_queues()[device].accept(std::move(camera)))report_native_camera_issue("command_camera_overflow");
}
inline std::optional<WiderViewState> capture_consume_triangle(const void* device){
    auto found=command_camera_queues().find(device);
    if(found!=command_camera_queues().end()){
        auto stamp=found->second.consume();if(stamp.accepted)return std::move(stamp.camera);
    }
    report_native_camera_issue("command_camera_missing");
    return WiderViewState{}; // Explicit invalid camera, never a live/default lens.
}
inline void capture_reset_commands(const void* device){command_camera_queues().erase(device);}
inline void capture_reset_all_commands(){command_camera_queues().clear();}
template<class Poly,class Vertex>
void capture_triangle(const Poly& poly,const Vertex* vertex,RenderState state,uint32_t mode1,const std::array<uint16_t,64>& fog,uint32_t fog_mask) {
    if (!capture_enabled()) return;
    auto stamp=executing_command_camera();
    if(!stamp.accepted){report_native_camera_issue("command_execution_unstamped");stamp.camera=WiderViewState{};}
    const auto& camera=stamp.camera;
    EmulationScope capture_profile("<capture>");
    RawTriangle triangle;triangle.fog_table=fog;triangle.fog_delta_mask=fog_mask;
    triangle.state=state; triangle.texture_mode1=mode1;
    triangle.alpha_reference=poly.alpharef; triangle.depth_constant=poly.zacolor;
    triangle.clip_left=poly.clipleft; triangle.clip_right=poly.clipright;
    triangle.clip_top=poly.cliptop; triangle.clip_bottom=poly.clipbottom;
    triangle.texture[0]=capture_texture(poly.tex0,state.texture_mode);
    triangle.texture[1]=capture_texture(poly.tex1,mode1);
    const double center=(state.color_path&(1u<<26))?.5:0.;
    for (size_t i=0;i<3;i++) {
        auto& v=triangle.vertex[i];
        v.x=vertex[i].x; v.y=vertex[i].y;
        const double dx=v.x-(poly.ax>>4)-center,dy=v.y-(poly.ay>>4)-center;
        v.z=(poly.startz+dx*poly.dzdx+dy*poly.dzdy)/4096.;
        v.q=(poly.startw+dx*poly.dwdx+dy*poly.dwdy)/4294967296.;
        v.r=(poly.startr+dx*poly.drdx+dy*poly.drdy)/4096.;
        v.g=(poly.startg+dx*poly.dgdx+dy*poly.dgdy)/4096.;
        v.b=(poly.startb+dx*poly.dbdx+dy*poly.dbdy)/4096.;
        v.a=(poly.starta+dx*poly.dadx+dy*poly.dady)/4096.;
        if (poly.tex0) {
            v.s[0]=(poly.starts0+dx*poly.ds0dx+dy*poly.ds0dy)/4294967296.;
            v.t[0]=(poly.startt0+dx*poly.dt0dx+dy*poly.dt0dy)/4294967296.;
            v.tq[0]=(poly.startw0+dx*poly.dw0dx+dy*poly.dw0dy)/4294967296.;
        }
        if (poly.tex1) {
            v.s[1]=(poly.starts1+dx*poly.ds1dx+dy*poly.ds1dy)/4294967296.;
            v.t[1]=(poly.startt1+dx*poly.dt1dx+dy*poly.dt1dy)/4294967296.;
            v.tq[1]=(poly.startw1+dx*poly.dw1dx+dy*poly.dw1dy)/4294967296.;
        }
    }
    voodoo_capture().triangle(uint64_t(reinterpret_cast<uintptr_t>(poly.destbase)),std::move(triangle),camera?&*camera:nullptr);
}
inline void capture_clear(const void* buffer,uint32_t color) {
    if (capture_enabled()) {
        voodoo_capture().clear(uint64_t(reinterpret_cast<uintptr_t>(buffer)),color);
        invalidate_captured_textures();
    }
}
inline void capture_lfb_pixel(const void* buffer,int x,int y,uint16_t pixel,uint32_t width,uint32_t height,uint16_t depth=65535) {
    if (!capture_enabled()) return;
    const uint32_t color=0xff000000|((((pixel>>11)&31)*255/31)<<16)
        |((((pixel>>5)&63)*255/63)<<8)|((pixel&31)*255/31);
    voodoo_capture().lfb_pixel(uint64_t(reinterpret_cast<uintptr_t>(buffer)),x,y,color,width,height,depth);
}
inline void capture_present(const void* buffer,uint32_t width,uint32_t height,int xoff,int yoff,uint32_t rowpixels) {
    if (!capture_enabled()) return;
    auto frame=voodoo_capture().present(uint64_t(reinterpret_cast<uintptr_t>(buffer)),width,height,xoff,yoff);
    invalidate_captured_textures();
    (void)rowpixels;
}
}

