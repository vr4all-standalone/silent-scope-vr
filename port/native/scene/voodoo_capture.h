// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "scene_bridge.h"
#include "native_camera.h"
#include <atomic>
#include <chrono>
#include <map>
#include <unordered_set>

namespace silent_scope::scene {
// Preserve projected hardware data until the native camera/depth scale is measured.
struct RawVertex {
    double x{},y{},z{},q{};
    double s[2]{},t[2]{},tq[2]{};
    double r{},g{},b{},a{};
};
struct RawTriangle {
    std::array<uint16_t,64> fog_table{};uint32_t fog_delta_mask{255};bool overlay{};
    std::array<RawVertex,3> vertex;
    RenderState state;
    uint32_t texture_mode1{}, alpha_reference{}, depth_constant{};
    uint32_t clip_left{},clip_right{},clip_top{},clip_bottom{};
    std::array<std::shared_ptr<const TextureImage>,2> texture;
};
struct RawFrame {
    uint64_t sequence{},scene_sequence{};
    double scene_seconds{}; // wall time of the native Voodoo buffer swap, retained through raster composition
    uint32_t width{},height{},clear_color{};
    int32_t x_offset{},y_offset{};
    bool complete{true};
    NativeLens lens;
    bool camera_owned{},wider_applied{};
    double wider_factor{1};
    std::vector<uint32_t> background;
    std::vector<uint32_t> native_raster;
    std::vector<uint16_t> background_depth;
    std::vector<RawTriangle> triangles;
};

// Called only by the emulation thread. Readers acquire complete immutable snapshots.
// At most three native buffers and one current published frame are retained here.
class VoodooCapture {
    const size_t triangle_limit_;
    const size_t texture_bytes_limit_;
    uint64_t sequence_{};
    NativeLens lens_;
    struct Pending {
        RawFrame frame;
        bool presented{};
        size_t texture_bytes{};
        std::unordered_set<const TextureImage*> textures;
    };
    std::map<uint64_t,Pending> buffers_;
    SnapshotSlot<const RawFrame> latest_;
    Pending& buffer(uint64_t key) {
        if (!buffers_.contains(key) && buffers_.size()>=3) buffers_.clear();
        return buffers_[key];
    }
    static void reset_pending(Pending& b) {
        auto triangles=std::move(b.frame.triangles);triangles.clear();
        auto textures=std::move(b.textures);textures.clear();
        b=Pending{};b.frame.triangles=std::move(triangles);b.textures=std::move(textures);
    }
    void begin_draw(Pending& b) {
        if (b.presented) {
            const auto color=b.frame.clear_color;
            const auto width=b.frame.width,height=b.frame.height;
            auto background=std::move(b.frame.background);
            auto depth=std::move(b.frame.background_depth);
            reset_pending(b);b.frame.clear_color=color;b.frame.width=width;b.frame.height=height;
            b.frame.background=std::move(background);b.frame.background_depth=std::move(depth);
        }
    }
public:
    explicit VoodooCapture(size_t triangle_limit=32768,size_t texture_bytes_limit=64*1024*1024)
        : triangle_limit_(triangle_limit),texture_bytes_limit_(texture_bytes_limit) {}
    void set_lens(NativeLens lens) { lens_=lens; }
    void clear(uint64_t key, uint32_t color) {
        auto& b=buffer(key);
        reset_pending(b);b.frame.clear_color=color;
    }
    void triangle(uint64_t key, RawTriangle triangle,const WiderViewState* camera=nullptr) {
        auto& b=buffer(key);
        // CarnEvil can fully redraw without issuing an RGB fastfill between swaps.
        begin_draw(b);
        if(camera){
            if(!b.frame.camera_owned){b.frame.camera_owned=true;b.frame.lens=camera->lens;b.frame.wider_applied=camera->applied;b.frame.wider_factor=camera->factor;}
            const auto& lens=b.frame.lens;
            if(!camera->lens.valid()||lens.width!=camera->lens.width||lens.height!=camera->lens.height
                ||lens.fx!=camera->lens.fx||lens.fy!=camera->lens.fy||lens.cx!=camera->lens.cx||lens.cy!=camera->lens.cy
                ||b.frame.wider_applied!=camera->applied||b.frame.wider_factor!=camera->factor){
                if(b.frame.complete)report_native_camera_issue(camera->lens.valid()?"capture_mixed":"capture_invalid");
                b.frame.complete=false;
            }
        }
        if (b.frame.triangles.size()>=triangle_limit_) { b.frame.complete=false; return; }
        size_t added=0;std::array<bool,2> fresh{};
        for(size_t unit=0;unit<triangle.texture.size();++unit){const auto& texture=triangle.texture[unit];
            fresh[unit]=texture&&!b.textures.contains(texture.get());
            if(fresh[unit])added+=texture->pixels.size()*4;
        }
        if (added>texture_bytes_limit_-b.texture_bytes) { b.frame.complete=false; return; }
        for(size_t unit=0;unit<triangle.texture.size();++unit)if(fresh[unit])b.textures.insert(triangle.texture[unit].get());
        b.texture_bytes+=added;
        b.frame.triangles.push_back(std::move(triangle));
    }
    void lfb_pixel(uint64_t key,int x,int y,uint32_t color,uint32_t width,uint32_t height,uint16_t depth=65535) {
        if (x<0 || y<0 || width>2048 || height>2048 || uint32_t(x)>=width || uint32_t(y)>=height) return;
        auto& b=buffer(key);
        begin_draw(b);
        if (b.frame.width!=width||b.frame.height!=height||b.frame.background.size()!=size_t(width)*height) {
            b.frame.background.assign(size_t(width)*height,b.frame.clear_color);
            b.frame.background_depth.assign(size_t(width)*height,65535);
            b.frame.width=width;b.frame.height=height;
        }
        const size_t index=size_t(y)*width+x;
        b.frame.background[index]=color;b.frame.background_depth[index]=depth;
    }
    std::shared_ptr<const RawFrame> present(uint64_t key,uint32_t width,uint32_t height,int xoff,int yoff) {
        auto& b=buffer(key);
        auto frame=b.frame;
        b.presented=true;
        frame.sequence=frame.scene_sequence=++sequence_;
        frame.scene_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        frame.width=width; frame.height=height;
        frame.x_offset=xoff; frame.y_offset=yoff;
        if(!frame.camera_owned)frame.lens=lens_;
        auto ready=std::make_shared<const RawFrame>(std::move(frame));
        latest_.store(ready,std::memory_order_release);
        return ready;
    }
    std::shared_ptr<const RawFrame> acquire() const { return latest_.load(std::memory_order_acquire); }
    void reset() { buffers_.clear(); lens_=NativeLens{}; latest_.store({},std::memory_order_release); }
};
inline VoodooCapture& voodoo_capture() { static VoodooCapture capture; return capture; }
}

