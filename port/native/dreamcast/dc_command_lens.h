// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_lens_ownership.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <mutex>
struct Sh4Context;
struct TA_context;
namespace silent_scope { namespace dreamcast {
struct CommandSpan{bool valid{};uint32_t begin{},length{},scratch{},scratch_length{},slot{},raw_begin{},raw_end{},poly_data{},poly_words{},strip_flags{},poly_type{},near_count{},references{},caller{},stack{};};
template<class Read> CommandSpan read_command_commit(uint32_t caller,uint32_t stack,uint32_t end,uint32_t,uint32_t r14,Read read){
    CommandSpan out;out.caller=caller;out.stack=stack;out.slot=read(0x8cd134e4);
    if((out.slot&0x1f000000u)!=0x0c000000u||(out.slot&0xffffff)>0xfffffbu)return out;
    out.raw_begin=read(out.slot);out.raw_end=(end&0x03ffffffu)|read(0x8cd134e0);
    if((out.raw_begin&0x1f000000u)!=0x0c000000u||(out.raw_end&0x1f000000u)!=0x0c000000u)return out;
    out.begin=out.raw_begin&0xffffffu;const uint32_t finish=out.raw_end&0xffffffu;
    if(finish<=out.begin||finish-out.begin>0x80000u||(out.begin&31)||(finish&31))return out;
    out.length=finish-out.begin;out.valid=true;
    if(caller==0x8c1124b0&&(stack&0x1f000000u)==0x0c000000u&&(stack&0xffffffu)<=0xffffb0u){
        const uint32_t scratch=read(stack+0x28);
        if((scratch&0x1f000000u)==0x0c000000u){
            out.scratch=scratch&0xffffffu;out.near_count=read(stack+0x2c);
            out.poly_data=read(stack);out.poly_type=read(stack+0x30);out.poly_words=r14?uint16_t(r14)-1:0;
            // R13 has already been reused as0fad24 function pointer. The
            // original stripcount/UFO word is immediately before SP0's data.
            if((out.poly_data&0x1f000000u)==0x0c000000u&&(out.poly_data&0xffffffu)>=2){
                const uint32_t address=out.poly_data-2;
                out.strip_flags=uint16_t(read(address&~3u)>>((address&2)*8));
            }
        }
    }
    return out;
}
// Source111fbc..fc2 holds payload word count inR14 and strip count/UFO inR13;
// 13bed0..bf08 masks count0x3fff and extra words(top2bits). Native types40
// consume index words;41/42 add two UV words per vertex. UFO words follow
// each triangle's third/subsequent vertex, not the first two vertices.
template<class Ledger,class Read,class Verify> SubmittedLens resolve_command_lens(CommandSpan& span,const Ledger& ledger,Read read,Verify exact_packet){
    if(!span.valid||!span.scratch||!span.poly_words||span.poly_words>65534
        ||(span.poly_type!=0x40&&span.poly_type!=0x41&&span.poly_type!=0x42)
        ||(span.poly_data&0x1f000000u)!=0x0c000000u||(span.poly_data&1))return {};
    const uint64_t finish=uint64_t(span.poly_data)+uint64_t(span.poly_words)*2;
    if((span.poly_data&0xffffffu)+uint64_t(span.poly_words)*2>0x1000000)return {};
    uint64_t cursor=span.poly_data;SubmittedLens lens;unsigned references=0,max_index=0;
    auto word=[&](uint64_t address){const uint32_t value=read(uint32_t(address)&~3u);return uint16_t(value>>((address&2)*8));};
    const unsigned strips=span.strip_flags&0x3fff,extra=span.strip_flags>>14;
    for(unsigned strip=0;strip<strips;++strip){
        if(cursor+2>finish)return {};
        const int count=int16_t(word(cursor));cursor+=2;const unsigned vertices=count<0?unsigned(-count):unsigned(count);
        if(vertices<3)return {};
        for(unsigned v=0;v<vertices;++v){
            const unsigned words=(span.poly_type==0x40?1:3)+(v>=2?extra:0);
            if(cursor+words*2>finish)return {};
            const unsigned index=word(cursor);cursor+=words*2;max_index=std::max(max_index,index);
            if(uint64_t(span.scratch)+uint64_t(index+1)*32>0x1000000)return {};
            const auto owner=ledger.lookup(span.scratch+index*32,32);
            if(!owner.valid||!exact_packet(span.scratch+index*32,32)||(lens.valid&&!same_projection(lens,owner)))return {};
            lens=owner;++references;
        }
    }
    // Actual native model streams align an odd payload with one zero word.
    if(cursor+2==finish&&word(cursor)==0)cursor+=2;
    if(cursor!=finish||!references)return {};
    span.references=references;span.scratch_length=(max_index+1)*32;return lens;
}
class CommandLensProbe{
    mutable std::mutex mutex_;
    std::filesystem::path path_;bool enabled_{},all_decoded_{};unsigned all_records_{};double end_native_{60};std::array<std::array<unsigned,2>,4> counts_{};std::array<std::array<double,2>,2> sampled_{{{{-1,-1}},{{-1,-1}}}};
public:
    explicit CommandLensProbe(const std::filesystem::path& dir,bool all_decoded=false,double end_native=60):all_decoded_(all_decoded){
        if(std::isfinite(end_native)&&end_native>=60&&end_native<=80)end_native_=end_native;
        std::error_code ec;enabled_=dir.is_absolute()&&std::filesystem::is_directory(dir,ec)&&!ec;
        static std::atomic<unsigned> nonce{};path_=dir/("dc-command-lens-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(nonce.fetch_add(1))+".jsonl");
        if(std::filesystem::exists(path_,ec)||ec)enabled_=false;
    }
    bool wants_locked(unsigned stage,double t,bool rtt=false)const{
        if(!enabled_||stage>3||!std::isfinite(t)||t<40||t>=end_native_)return false;
        if(stage==3&&all_decoded_&&!rtt)return all_records_<2000;
        if((t>=44.99&&t<45.09)||(t>=54.99&&t<55.09))return counts_[stage][t<50?0:1]<2048;
        return stage>=2&&t-sampled_[stage-2][rtt?1:0]>=1;
    }
    bool enabled()const{std::lock_guard<std::mutex> lock(mutex_);return enabled_;}
    bool wants(unsigned stage,double t,bool rtt=false)const{std::lock_guard<std::mutex> lock(mutex_);return wants_locked(stage,t,rtt);}
    void record(unsigned stage,double t,uint64_t generation,uint64_t sequence,uint32_t ta,const CommandSpan& span,
        const SubmittedLens& lens,bool rejected=false,unsigned triangles=0,bool rtt=false)noexcept{
        std::lock_guard<std::mutex> lock(mutex_);
        if(!wants_locked(stage,t,rtt))return;
        try{
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return;}out.imbue(std::locale::classic());
            out<<std::setprecision(12)<<"{\"stage\":"<<stage<<",\"native_seconds\":"<<t<<",\"generation\":"<<generation<<",\"submit\":"<<sequence
                <<",\"ta\":"<<ta<<",\"begin\":"<<span.begin<<",\"length\":"<<span.length<<",\"scratch\":"<<span.scratch
                <<",\"scratch_length\":"<<span.scratch_length<<",\"slot\":"<<span.slot<<",\"valid\":"<<(lens.valid?"true":"false")
                <<",\"poly_data\":"<<span.poly_data<<",\"poly_words\":"<<span.poly_words<<",\"poly_type\":"<<span.poly_type
                <<",\"strip_flags\":"<<span.strip_flags<<",\"near_count\":"<<span.near_count<<",\"references\":"<<span.references
                <<",\"caller\":"<<span.caller<<",\"stack\":"<<span.stack
                <<",\"fx\":"<<lens.fx<<",\"fy\":"<<lens.fy<<",\"cx\":"<<lens.cx<<",\"cy\":"<<lens.cy
                <<",\"rejected\":"<<(rejected?"true":"false")<<",\"triangles\":"<<triangles<<",\"rtt\":"<<(rtt?"true":"false")<<"}\n";
            if(!out){enabled_=false;return;}if((t>=44.99&&t<45.09)||(t>=54.99&&t<55.09))++counts_[stage][t<50?0:1];
            if(stage==3&&all_decoded_&&!rtt)++all_records_;
            if(stage>=2)sampled_[stage-2][rtt?1:0]=t;
        }catch(...){enabled_=false;}
    }
    template<class Context,class Result> void record_decoded(const Context& ctx,const Result& result)noexcept{
        std::lock_guard<std::mutex> lock(mutex_);
        const double t=ctx.dcCamera.native_seconds;if(!wants_locked(3,t,ctx.isRTT))return;
        try{
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return;}out.imbue(std::locale::classic());
            out<<std::setprecision(12)<<"{\"stage\":3,\"native_seconds\":"<<t<<",\"generation\":"<<ctx.dcCamera.generation<<",\"submit\":"<<ctx.dcCamera.submit_sequence
                <<",\"valid\":"<<(result.lens.valid?"true":"false")<<",\"fx\":"<<result.lens.fx<<",\"rtt\":"<<(ctx.isRTT?"true":"false")
                <<",\"world\":"<<result.world_triangles<<",\"ui\":"<<result.ui_triangles<<",\"unknown\":"<<result.unknown_triangles<<",\"mixed\":"<<result.mixed_triangles
                <<",\"skipped_never\":"<<result.skipped_never_polygons<<",\"skipped_primers\":"<<result.skipped_primers
                <<",\"malformed\":"<<(result.malformed?"true":"false")<<",\"modifiers\":"<<(result.unsupported_modifiers?"true":"false");
            if(result.has_problem){
                const auto& list=result.problem_list==0?ctx.global_param_op:result.problem_list==1?ctx.global_param_pt:ctx.global_param_tr;
                const auto& p=list[result.problem_polygon];
                out<<",\"problem_list\":"<<result.problem_list<<",\"problem_poly\":"<<result.problem_polygon<<",\"pcw\":"<<p.pcw.full
                    <<",\"isp\":"<<p.isp.full<<",\"tsp\":"<<p.tsp.full<<",\"tcw\":"<<p.tcw.full<<",\"clip\":"<<p.tileclip<<",\"problem_vertices\":[";
                for(unsigned k=0;k<3;++k){const auto id=result.problem_vertices[k];const auto& v=ctx.verts[id];const auto& lens=ctx.dcVertexLens[id];if(k)out<<',';
                    out<<'['<<id<<','<<v.x<<','<<v.y<<','<<v.z<<','<<lens.fx<<','<<lens.request_revision<<']';}out<<']';
            }
            out<<"}\n";if(!out){enabled_=false;return;}
            if((t>=44.99&&t<45.09)||(t>=54.99&&t<55.09))++counts_[3][t<50?0:1];
            if(all_decoded_&&!ctx.isRTT)++all_records_;
            sampled_[1][ctx.isRTT?1:0]=t;
        }catch(...){enabled_=false;}
    }
};
inline CommandLensProbe& environment_command_lens_probe(){
    static CommandLensProbe probe([]{const char* p=std::getenv("SILENT_SCOPE_DC_OWNERSHIP_PROBE_DIR");return std::filesystem::path(p?p:"");}(),
        []{const char* p=std::getenv("SILENT_SCOPE_DC_OWNERSHIP_ALL_DECODED");return p&&std::strcmp(p,"1")==0;}(),
        []{const char* p=std::getenv("SILENT_SCOPE_DC_OWNERSHIP_END_SECONDS");return p?std::strtod(p,nullptr):60.;}());return probe;
}
void ensure_command_lens_traps(double now);
void reset_command_lens_traps(bool discard,bool restore);
bool handle_command_lens_trap(Sh4Context*,uint32_t pc);
void observe_ta_lens_packet(TA_context*,const void* data,bool vertex,bool end_strip,bool reset_strip,bool sprite);
void publish_ta_owned_lens(TA_context*,unsigned width,unsigned height,uint64_t generation,uint64_t sequence,double now);
SubmittedLens decoded_packet_lens(TA_context*,const void*);
void finalize_decoded_ta_lens(TA_context*);
}}
