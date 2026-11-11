// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Read-only diagnostic of camera age versus actual TA build/submission stages.
// These samples are NOT geometry ownership receipts and cannot enable a lens.
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <string_view>
namespace silent_scope { namespace dreamcast {
enum class LensBoundary : unsigned {ListInit,FirstPacket,Dma,Submit};
class LensBoundaryProbe {
    std::filesystem::path path_;unsigned maximum_{},records_{};bool enabled_{};
    std::array<unsigned,4> stages_{};
public:
    explicit LensBoundaryProbe(const std::filesystem::path& directory,unsigned maximum=8192):maximum_(maximum>8192?8192:maximum){
        std::error_code error;enabled_=directory.is_absolute()&&std::filesystem::is_directory(directory,error)&&!error;
        static std::atomic<unsigned> nonce{};
        const auto time=std::chrono::system_clock::now().time_since_epoch().count();
        path_=directory/("dc-lens-boundary-"+std::to_string(time)+"-"+std::to_string(nonce.fetch_add(1))+".jsonl");
        if(std::filesystem::exists(path_,error)||error)enabled_=false;
    }
    bool enabled()const{return enabled_;}
    unsigned records()const{return records_;}
    const std::filesystem::path& path()const{return path_;}
    template<class Read> bool record(std::string_view title,LensBoundary stage,double seconds,uint64_t generation,
        uint32_t ta,uint32_t context_pc,uint32_t source,uint32_t length,bool continuation,Read read)noexcept{
        const auto index=unsigned(stage);
        if(!enabled_||title!="T9507N"||!std::isfinite(seconds)||seconds<40||seconds>=60
            ||records_>=maximum_||index>=stages_.size()||stages_[index]>=2048)return false;
        if(stage==LensBoundary::Dma&&!((seconds>=44.8&&seconds<45.3)||(seconds>=54.8&&seconds<55.3)))return false;
        try{
            static constexpr const char* names[]{"list_init","first_packet","dma","submit"};
            static constexpr std::array<uint32_t,14> addresses{{
                0x8cd1f9c0,0x8cd1f9c4,0x8cd1f9c8,0x8cd1f9cc,0x8cd1f9d0,0x8cd1f9f4,0x8cd1f9f8,
                0x8cd206d4,0x8cd206d8,0x8cd206dc,0x8cd206e0,0x8c1b0108,0x8c1b010c,0x8c1b0114}};
            static constexpr const char* labels[]{"sdk_focal","sdk_width","sdk_height","sdk_cx","sdk_cy","sdk_scale_x","sdk_scale_y",
                "cache_fx","cache_fy","cache_cx","cache_cy","main_source","scope_source","mode"};
            std::array<uint32_t,14> words{};for(unsigned i=0;i<words.size();++i)words[i]=read(addresses[i]);
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return false;}out.imbue(std::locale::classic());
            out<<std::setprecision(12)<<"{\"stage\":\""<<names[index]<<"\",\"native_seconds\":"<<seconds
                <<",\"generation\":"<<generation<<",\"ta_address\":"<<ta<<",\"context_pc\":"<<context_pc
                <<",\"source\":"<<source<<",\"length\":"<<length<<",\"continuation\":"<<(continuation?"true":"false");
            for(unsigned i=0;i<words.size();++i)out<<",\""<<labels[i]<<"\":\""<<std::hex<<std::setw(8)<<std::setfill('0')<<words[i]<<std::dec<<'\"';
            out<<"}\n";if(!out){enabled_=false;return false;}
            ++records_;++stages_[index];return true;
        }catch(...){enabled_=false;return false;}
    }
};
inline LensBoundaryProbe& environment_lens_boundary_probe(){
    static LensBoundaryProbe probe([]{const char* path=std::getenv("SILENT_SCOPE_DC_LENS_BOUNDARY_DIR");return std::filesystem::path(path?path:"");}());return probe;
}
// Implemented in vr_reproject.cpp; SH4-thread hooks only. PC is the saved CPU
// context PC (possibly a JIT block PC), not asserted to be the exact instruction.
void probe_lens_boundary(LensBoundary stage,uint32_t ta,uint32_t source=0,uint32_t length=0,bool continuation=false);
}}
