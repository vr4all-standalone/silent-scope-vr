// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_scope.h"
#include "dc_scope_composite.h"
#include <cmath>
#include <cstring>
#include <chrono>
namespace silent_scope::dc_scope {
namespace {thread_local bool authorized_rifle_draw=false;thread_local double presentation_time=-1;}
bool rifle_draw_authorized()noexcept{return authorized_rifle_draw;}
bool set_rifle_draw_authorized(bool value)noexcept{const bool previous=authorized_rifle_draw;authorized_rifle_draw=value;return previous;}
double presentation_seconds()noexcept{return presentation_time>=0?presentation_time:monotonic_seconds();}
double set_presentation_seconds(double value)noexcept{const double previous=presentation_time;presentation_time=value;return previous;}
double monotonic_seconds() noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool is_native_scope(const Source& s) noexcept {
    return s.product && std::strcmp(s.product,"T9507N")==0 && s.is_rtt && s.native_copy &&
        s.width==640 && s.height==480 && s.destination==0x52c000 && s.pack_mode==1 && s.stride==512 &&
        s.clip_x==0 && s.clip_y==0 && s.clip_width==256 && s.clip_height==256 &&
        s.submit_sequence && s.native_generation && s.source_framebuffer;
}
bool ScopeGpu::initialize(GlApi api, uintptr_t context) noexcept {
    if(context_ || !context || !api.create || !api.copy || !api.destroy) return false;
    api_=api; context_=context; owner_=std::this_thread::get_id();
    images_={}; targets_={}; next_slot_=0; native_generation_=0; last_error_=0;status_={}; invalidate(); return true;
}
void ScopeGpu::set_physical_rifle_active(bool value) noexcept {
    if(active_.exchange(value)!=value) invalidate();
}
void ScopeGpu::invalidate() noexcept { lifecycle_.fetch_add(1); }
bool ScopeGpu::capture(const Source& source, uintptr_t context, double now) noexcept {
    if(!owns(context)) return false;
    // Other native RTT passes never replace or poison a compatible scope image.
    if(!active_.load() || !is_native_scope(source) || !std::isfinite(now) || now<0)return false;
    ++status_.attempts;
    if(native_generation_ && native_generation_!=source.native_generation)invalidate();
    native_generation_=source.native_generation;
    const auto lifecycle=lifecycle_.load();
    for(const auto& image:images_)if(image.texture&&image.lifecycle_generation==lifecycle&&
        (now<image.captured_seconds||source.submit_sequence<=image.submit_sequence))return false;
    auto& image=images_[next_slot_];auto& target=targets_[next_slot_];
    image={};
    try {
        if(!target.texture) target=api_.create();
        if(!target.texture || !target.framebuffer){++status_.failed;return false;}
        const auto result=api_.copy(target,source.source_framebuffer); last_error_=result.gl_error;
        if(!result.copied || !active_.load() || lifecycle_.load()!=lifecycle){++status_.failed;return false;}
        image={target.texture,256,256,++sequence_,source.submit_sequence,source.native_generation,
                lifecycle,context_,now,true,true};
        next_slot_=(next_slot_+1)%targets_.size();
        ++status_.copied;status_.last_submit=source.submit_sequence;status_.last_seconds=now;
        return true;
    } catch(...) { image={};++status_.failed;return false; }
}
TextureView ScopeGpu::acquire(uint64_t main_sequence,uint64_t generation,uintptr_t context,double now) const noexcept {
    if(!owns(context)||!active_.load()||generation!=native_generation_||!std::isfinite(now))return {};
    const auto lifecycle=lifecycle_.load();
    TextureView result;
    for(const auto& image:images_){
        if(!image.texture||lifecycle!=image.lifecycle_generation||generation!=image.native_generation||
           now<image.captured_seconds||now-image.captured_seconds>.25||main_sequence<image.submit_sequence||
           main_sequence-image.submit_sequence>4)continue;
        if(image.submit_sequence>result.submit_sequence)result=image;
    }
    if(lifecycle_.load()!=lifecycle||!active_.load())return {};
    return result;
}
bool ScopeGpu::release(uintptr_t context) noexcept {
    if(!owns(context)) { invalidate(); return false; }
    images_={}; invalidate();
    try { for(auto target:targets_)if(target.texture||target.framebuffer)api_.destroy(target); }
    catch(...) { return false; }
    targets_={}; context_=0; owner_={}; api_={}; native_generation_=0; return true;
}
void ScopeGpu::context_lost() noexcept {
    if(context_ && owner_!=std::this_thread::get_id()) { invalidate(); return; }
    invalidate(); images_={}; targets_={}; context_=0; owner_={}; api_={}; native_generation_=0;
}
bool ScopeGpu::owns(uintptr_t context) const noexcept {
    return context_ && context_==context && owner_==std::this_thread::get_id();
}
ScopeGpu& gpu_scope() noexcept { static ScopeGpu owner; return owner; }
extern "C" void silent_scope_dc_scope_active(bool active) noexcept { gpu_scope().set_physical_rifle_active(active); }
extern "C" void silent_scope_dc_scope_invalidate() noexcept { gpu_scope().invalidate(); }
extern "C" bool silent_scope_dc_scope_view(uint64_t sequence,uint64_t generation,uintptr_t context,double now,TextureView* out) noexcept {
    if(!out)return false;
    *out=gpu_scope().acquire(sequence,generation,context,now);
    return bool(*out);
}
}
