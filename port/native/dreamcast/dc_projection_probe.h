// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Diagnostic only: actual projector argument headers, never a live-SDK lens
// guess. Header-derived output ranges need native DMA/assembler correlation
// before they may publish production ownership.
#include "dc_lens.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
struct Sh4Context;
namespace silent_scope { namespace dreamcast {
constexpr std::array<std::pair<uint32_t,uint32_t>,8> ProjectionSites{{
    {0x8c0fbe28,0xffebfffbu},{0x8c0fbf14,0xffebfffbu},{0x8c0fbff4,0xffebfffbu},
    {0x8c0fc0c4,0xffebfffbu},{0x8c0fc208,0xffebfffbu},{0x8c0fc340,0xffebfffbu},
    {0x8c112566,0x032ed409u},{0x8c124dc4,0xffebfffbu}}};
inline int projection_site(uint32_t pc){for(unsigned i=0;i<ProjectionSites.size();++i)if(ProjectionSites[i].first==pc)return int(i);return -1;}
struct ProjectionEntry {
    uint32_t begin{},length{},count{},mode{},source_main{},raw_destination{};
    float fx{},fy{},cx{},cy{};
    bool linear{},candidate{};
};
template<class Read> ProjectionEntry read_projection_entry(uint32_t pc,uint32_t r4,uint32_t fpscr,Read read){
    ProjectionEntry out;const int site=projection_site(pc);if(site<0)return out;
    const bool chunk=site>=6;const uint32_t header=chunk?0x8cd206b0:r4;
    if((header&0x1f000000u)!=0x0c000000u||(header&0xffffffu)>0xffffc0u)return out;
    out.count=read(header+(chunk?0x14:0));out.raw_destination=read(header+(chunk?0x1c:4));
    out.fx=lens_float(read(header+(chunk?0x24:16)));out.fy=lens_float(read(header+(chunk?0x28:20)));
    out.cx=lens_float(read(chunk?header+0x2c:LensSdkBase+12));out.cy=lens_float(read(chunk?header+0x30:LensSdkBase+16));
    out.mode=read(LensMode);out.source_main=read(LensSourceMain);
    out.begin=out.raw_destination&0xffffffu;
    const uint64_t bytes=uint64_t(out.count)*(chunk?32:64);
    out.linear=site!=7&&out.count&&out.count<=65536&&(out.raw_destination&0x1f000000u)==0x0c000000u
        &&uint64_t(out.begin)+bytes<=0x1000000;
    if(out.linear)out.length=uint32_t(bytes);
    out.candidate=out.linear&&!(fpscr&((1u<<19)|(1u<<20)))&&out.mode==0&&out.source_main==0x440ac000u
        &&out.cx==320&&out.cy==240&&((out.fx==555&&out.fy==555)||(out.fx==185&&out.fy==185));
    return out;
}
class ProjectionProbe {
    std::filesystem::path path_;bool enabled_{};std::array<unsigned,2> records_{};
public:
    explicit ProjectionProbe(const std::filesystem::path& directory){
        std::error_code error;enabled_=directory.is_absolute()&&std::filesystem::is_directory(directory,error)&&!error;
        static std::atomic<unsigned> nonce{};const auto time=std::chrono::system_clock::now().time_since_epoch().count();
        path_=directory/("dc-projector-"+std::to_string(time)+"-"+std::to_string(nonce.fetch_add(1))+".jsonl");
        if(std::filesystem::exists(path_,error)||error)enabled_=false;
    }
    bool enabled()const{return enabled_;}
    bool wants(double t)const{
        return enabled_&&std::isfinite(t)&&((t>=44.8&&t<45.3&&records_[0]<2048)||(t>=54.8&&t<55.3&&records_[1]<2048));
    }
    bool record(double t,uint64_t generation,uint32_t pc,uint32_t caller,uint32_t r4,uint32_t subtype,
        uint32_t fpscr,bool guard,const ProjectionEntry& value)noexcept{
        if(!wants(t))return false;
        try{
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return false;}out.imbue(std::locale::classic());
            out<<std::setprecision(12)<<"{\"native_seconds\":"<<t<<",\"generation\":"<<generation<<",\"pc\":"<<pc
                <<",\"caller\":"<<caller<<",\"r4\":"<<r4<<",\"subtype\":"<<subtype<<",\"fpscr\":"<<fpscr
                <<",\"code_guard\":"<<(guard?"true":"false")<<",\"linear\":"<<(value.linear?"true":"false")
                <<",\"candidate\":"<<(value.candidate?"true":"false")<<",\"begin\":"<<value.begin<<",\"length\":"<<value.length
                <<",\"count\":"<<value.count<<",\"mode\":"<<value.mode<<",\"main_source\":"<<value.source_main;
            auto scalar=[&](const char* name,float v){out<<",\""<<name<<"\":";if(std::isfinite(v))out<<v;else out<<"null";};
            scalar("fx",value.fx);scalar("fy",value.fy);scalar("cx",value.cx);scalar("cy",value.cy);out<<"}\n";
            if(!out){enabled_=false;return false;}++records_[t<50?0:1];return true;
        }catch(...){enabled_=false;return false;}
    }
};
inline ProjectionProbe& environment_projection_probe(){
    static ProjectionProbe probe([]{const char* p=std::getenv("SILENT_SCOPE_DC_PROJECTOR_PROBE_DIR");return std::filesystem::path(p?p:"");}());return probe;
}
bool handle_projection_probe_trap(Sh4Context* ctx,uint32_t guest_pc);
void ensure_projection_probe_traps(double now);
void reset_projection_probe_traps(bool discard,bool restore);
}}
