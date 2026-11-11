// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// T9507N discovery only. These are immutable submission-time SDK candidate
// values, not a measured lens/profile and never permission to change game RAM.
#include "dc_camera_types.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string>
#include <utility>
namespace silent_scope { namespace dreamcast {

class CameraSampler {
    std::atomic<uint64_t> generation_{1},sequence_{};
public:
    static constexpr uint32_t SdkBase=0x8cd1f9c0,ObjectBase=0x8cd134f4,CacheBase=0x8cd206d4;
    // Actual US1ST_READ SHA2563cf1879f... Ninja vertex projection and culling.
    static constexpr std::array<std::pair<uint32_t,uint32_t>,24> CodeGuard{{
        {0x8c0fbe3c,0xdc2d2f86u},{0x8c0fbe40,0xfac8dd2du},
        {0x8c0fbebc,0xf082f192u},{0x8c0fbec0,0xf032f132u},
        {0x8c0fbec4,0xf0a0f1b0u},{0x8c0fbef4,0x8cd1f9ccu},
        {0x8c0fbef8,0x8cd1f9d0u},{0x8c0fc4ac,0x6102d30du},
        {0x8c0fc4b0,0xf3382212u},{0x8c0fc4b4,0xd20df2a8u},
        {0x8c0fc4b8,0xfc2af232u},{0x8c0fc4bc,0xf2a8f328u},
        {0x8c0fc4c0,0xfb2af232u},{0x8c0fc4dc,0x8cd1f9c0u},
        {0x8c0fc4e0,0x8cd13508u},{0x8c0fc4e8,0x8cd1350cu},
        // Actual cache refresh, cached chunk projector and current-focal culler.
        {0x8c111cf4,0xe004d59bu},{0x8c111d08,0xf258f338u},
        {0x8c111d14,0xf232f328u},{0x8c111d20,0xe010f437u},
        {0x8c111d28,0xf437000bu},{0x8c14066c,0xf149f049u},
        {0x8c140670,0xf349f249u},{0x8c124f32,0xf228f118u}}};
    uint64_t generation()const{return generation_.load(std::memory_order_relaxed);}
    void reset_generation(){generation_.fetch_add(1,std::memory_order_relaxed);}
    template<class Read> CameraContext capture(const std::string& product,uint32_t address,bool rtt,uint32_t destination,bool candidates,Read read){
        CameraContext out;if(product!="T9507N")return out;
        out.supported=true;out.is_rtt=rtt;out.ta_address=address;out.destination=destination&0x7fffff;
        out.generation=generation_.load(std::memory_order_relaxed);out.submit_sequence=sequence_.fetch_add(1,std::memory_order_relaxed)+1;
        if(!candidates)return out;
        for(auto guard:CodeGuard){const uint32_t actual=read(guard.first);if(actual!=guard.second){
            out.failed_guard_address=guard.first;out.failed_guard_expected=guard.second;out.failed_guard_actual=actual;return out;}}
        out.code_guard_valid=true;
        for(unsigned i=0;i<out.sdk_words.size();++i)out.sdk_words[i]=read(SdkBase+i*4);
        for(unsigned i=0;i<out.object_words.size();++i)out.object_words[i]=read(ObjectBase+i*4);
        auto scalar=[&](uint32_t address){const uint32_t bits=read(address);float value;std::memcpy(&value,&bits,4);return value;};
        out.cache_fx=scalar(CacheBase);out.cache_fy=scalar(CacheBase+4);out.cache_cx=scalar(CacheBase+8);out.cache_cy=scalar(CacheBase+12);out.cache_captured=true;
        out.candidates_captured=true;return out;
    }
};
struct CameraProbeLimits {unsigned max_records{128};double begin_native{30},end_native{70},interval_native{1};};
// Explicit diagnostic override; production identities still stamp with probes off.
inline CameraProbeLimits ui_camera_probe_limits(const char* flag,const char* end_override=nullptr){
    if(flag&&std::strcmp(flag,"1")==0)return CameraProbeLimits{40,3,20,1};
    CameraProbeLimits limits;
    const double end=end_override?std::strtod(end_override,nullptr):70;
    if(std::isfinite(end)&&end>=70&&end<=80)limits.end_native=end;
    return limits;
}
class CameraProbe {
    std::filesystem::path path_;CameraProbeLimits limits_;bool enabled_{};unsigned records_{};
    uint64_t generation_{};std::array<double,2> sampled_{{-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}};
public:
    explicit CameraProbe(std::filesystem::path directory,CameraProbeLimits limits={}):limits_(limits){
        std::error_code error;enabled_=directory.is_absolute()&&std::filesystem::is_directory(directory,error)&&!error;
        limits_.max_records=limits_.max_records>128?128:limits_.max_records;
        if(!std::isfinite(limits_.begin_native)||!std::isfinite(limits_.end_native)||limits_.begin_native<0||limits_.end_native<=limits_.begin_native)enabled_=false;
        if(!std::isfinite(limits_.interval_native)||limits_.interval_native<1)limits_.interval_native=1;
        const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();static std::atomic<unsigned> nonce{};
        for(unsigned i=0;i<32&&enabled_;++i){path_=directory/("dc-camera-"+std::to_string(stamp)+"-"+std::to_string(nonce.fetch_add(1))+".jsonl");
            if(!std::filesystem::exists(path_,error)&&!error)return;}
        enabled_=false;
    }
    bool enabled()const{return enabled_;}
    bool wants_candidates(bool rtt,double time,uint64_t generation)const{
        return enabled_&&records_<limits_.max_records&&std::isfinite(time)&&time>=limits_.begin_native&&time<limits_.end_native
            &&(generation!=generation_||time-sampled_[rtt?1:0]>=limits_.interval_native);
    }
    unsigned records()const{return records_;}
    const std::filesystem::path& path()const{return path_;}
    bool record(const CameraContext& stamp,double native_seconds)noexcept{
        try{
            if(!enabled_||!stamp.supported||records_>=limits_.max_records||!std::isfinite(native_seconds)||native_seconds<limits_.begin_native||native_seconds>=limits_.end_native)return false;
            if(generation_!=stamp.generation){generation_=stamp.generation;sampled_.fill(-std::numeric_limits<double>::infinity());}
            const unsigned kind=stamp.is_rtt?1:0;if(native_seconds-sampled_[kind]<limits_.interval_native)return false;
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return false;}out.imbue(std::locale::classic());
            out<<std::setprecision(12)<<"{\"generation\":"<<stamp.generation<<",\"submit_sequence\":"<<stamp.submit_sequence
                <<",\"native_seconds\":"<<native_seconds<<",\"kind\":\""<<(stamp.is_rtt?"rtt":"main")<<"\",\"ta_address\":"<<stamp.ta_address
                <<",\"destination\":"<<stamp.destination<<",\"code_guard_valid\":"<<(stamp.code_guard_valid?"true":"false")
                <<",\"candidates_captured\":"<<(stamp.candidates_captured?"true":"false")<<",\"failed_guard_address\":"<<stamp.failed_guard_address
                <<",\"failed_guard_expected\":"<<stamp.failed_guard_expected<<",\"failed_guard_actual\":"<<stamp.failed_guard_actual;
            auto words=[&](const char* name,uint32_t address,const auto& values){out<<",\""<<name<<"_address\":"<<address<<",\""<<name<<"_words\":[";
                for(unsigned i=0;i<values.size();++i){if(i)out<<',';out<<'\"'<<std::hex<<std::setw(8)<<std::setfill('0')<<values[i]<<std::dec<<'\"';}out<<']';};
            words("sdk",CameraSampler::SdkBase,stamp.sdk_words);words("object",CameraSampler::ObjectBase,stamp.object_words);
            out<<",\"cache_address\":"<<CameraSampler::CacheBase<<",\"cache_captured\":"<<(stamp.cache_captured?"true":"false");
            auto cache=[&](const char* name,float value){out<<",\"cache_"<<name<<"\":";if(std::isfinite(value))out<<value;else out<<"null";};
            cache("fx",stamp.cache_fx);cache("fy",stamp.cache_fy);cache("cx",stamp.cache_cx);cache("cy",stamp.cache_cy);
            out<<"}\n";if(!out){enabled_=false;return false;}sampled_[kind]=native_seconds;++records_;return true;
        }catch(...){enabled_=false;return false;}
    }
};
inline CameraProbe& environment_camera_probe(){
    static CameraProbe probe([]{const char* path=std::getenv("SILENT_SCOPE_DC_CAMERA_PROBE_DIR");return std::filesystem::path(path?path:"");}(),
        ui_camera_probe_limits(std::getenv("SILENT_SCOPE_DC_UI_PROBE"),std::getenv("SILENT_SCOPE_DC_CAMERA_PROBE_END_SECONDS")));return probe;
}
}}
