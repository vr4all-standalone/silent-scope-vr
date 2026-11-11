// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_camera_types.h"
#include <algorithm>
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
#include <locale>
#include <sstream>
#include <vector>
namespace silent_scope { namespace dreamcast {
struct CameraGeometryProbeLimits {unsigned records{40};double begin{40},end{60};};
inline CameraGeometryProbeLimits camera_geometry_probe_limits(bool ui,const char* end_override){
    if(ui)return {40,3,20};
    const double end=end_override?std::strtod(end_override,nullptr):60;
    if(std::isfinite(end)&&end>60&&end<=80)return {80,40,end};
    return {};
}
// Read-only native positions/indices/material words, tied to the immutable SDK
// diagnostic sample. This is a diagnostic format, not a production scene ABI.
class CameraGeometryProbe {
    std::filesystem::path directory_;bool enabled_{};unsigned limit_{},records_{};double begin_{40},end_{60};
    uint64_t generation_{};std::array<uint64_t,2> last_sequence_{};
    std::vector<std::filesystem::path> paths_;
    static void word(std::ostream& stream,uint32_t value){
        const char bytes[4]={char(value),char(value>>8),char(value>>16),char(value>>24)};stream.write(bytes,4);
    }
    static void scalar(std::ostream& stream,float value){uint32_t bits;static_assert(sizeof(bits)==sizeof(value),"IEEE32 required");std::memcpy(&bits,&value,4);word(stream,bits);}
public:
    explicit CameraGeometryProbe(std::filesystem::path directory,unsigned maxRecords=40,double begin=40,double end=60):directory_(std::move(directory)),limit_(std::min(maxRecords,80u)),begin_(begin),end_(end){
        std::error_code error;enabled_=directory_.is_absolute()&&std::filesystem::is_directory(directory_,error)&&!error;
        if(!std::isfinite(begin_)||!std::isfinite(end_)||begin_<0||end_<=begin_)enabled_=false;
    }
    unsigned records()const{return records_;}
    const std::vector<std::filesystem::path>& paths()const{return paths_;}
    template<class Context> bool record(const Context& ctx){
        const auto& camera=ctx.dcCamera;
        if(!enabled_||records_>=limit_||!camera.supported||!camera.candidates_captured||!camera.code_guard_valid
            ||!std::isfinite(camera.native_seconds)||camera.native_seconds<begin_||camera.native_seconds>=end_)return false;
        if(camera.generation!=generation_){generation_=camera.generation;last_sequence_={};}
        const auto kind=camera.is_rtt?1u:0u;
        if(!camera.submit_sequence||camera.submit_sequence<=last_sequence_[kind])return false;
        const auto polygons=ctx.global_param_op.size()+ctx.global_param_pt.size()+ctx.global_param_tr.size();
        if(ctx.verts.size()>65536||ctx.idx.size()>262144||polygons>16384||ctx.render_passes.size()>64||ctx.sortedTriangles.size()>16384)return false;
        // Native TA strip conversion emits fixed-index primitive restart.
        // Preserve that sentinel; every other index must reference a vertex.
        for(auto index:ctx.idx)if(index!=0xffffffffu&&index>=ctx.verts.size())return false;
        const auto ranges=[&](const auto& params){for(const auto& p:params)if(uint64_t(p.first)+p.count>ctx.idx.size())return false;return true;};
        if(!ranges(ctx.global_param_op)||!ranges(ctx.global_param_pt))return false;
        uint32_t previousTr=0,previousSorted=0,previousOp=0,previousPt=0;
        for(const auto& pass:ctx.render_passes){
            if(pass.tr_count<previousTr||pass.tr_count>ctx.global_param_tr.size()||pass.sorted_tr_count<previousSorted||pass.sorted_tr_count>ctx.sortedTriangles.size()
                ||pass.op_count<previousOp||pass.op_count>ctx.global_param_op.size()||pass.pt_count<previousPt||pass.pt_count>ctx.global_param_pt.size())return false;
            // sortTriangles leaves TR params as original contiguous vertex
            // strips. Its sorted draw batches reference the separate idx array.
            // Even an empty sorted pass appends a dummy SortedTriangle record.
            const bool sorted=pass.sorted_tr_count>previousSorted;
            if(sorted&&!pass.autosort)return false;
            for(uint32_t i=previousTr;i<pass.tr_count;++i){const auto& p=ctx.global_param_tr[i];
                if(uint64_t(p.first)+p.count>(sorted?ctx.verts.size():ctx.idx.size()))return false;}
            for(uint32_t i=previousSorted;i<pass.sorted_tr_count;++i){const auto& p=ctx.sortedTriangles[i];
                if(p.polyIndex>=ctx.global_param_tr.size()||uint64_t(p.first)+p.count>ctx.idx.size()||p.count%3)return false;}
            previousTr=pass.tr_count;previousSorted=pass.sorted_tr_count;previousOp=pass.op_count;previousPt=pass.pt_count;
        }
        if(previousTr!=ctx.global_param_tr.size()||previousSorted!=ctx.sortedTriangles.size())return false;
        std::ostringstream metadata;metadata.imbue(std::locale::classic());metadata<<std::setprecision(17)
            <<"{\"generation\":"<<camera.generation<<",\"submit_sequence\":"<<camera.submit_sequence
            <<",\"native_seconds\":"<<camera.native_seconds<<",\"kind\":\""<<(camera.is_rtt?"rtt":"main")
            <<"\",\"destination\":"<<camera.destination<<",\"sdk_words\":[";
        for(unsigned i=0;i<camera.sdk_words.size();++i){if(i)metadata<<',';metadata<<'"'<<std::hex<<std::setw(8)<<std::setfill('0')<<camera.sdk_words[i]<<'"';}
        metadata<<"],\"cache_captured\":"<<(camera.cache_captured?"true":"false")<<std::dec<<std::setfill(' ');
        const auto cache=[&](const char* name,float value){metadata<<",\"cache_"<<name<<"\":";if(std::isfinite(value))metadata<<value;else metadata<<"null";};
        cache("fx",camera.cache_fx);cache("fy",camera.cache_fy);cache("cx",camera.cache_cx);cache("cy",camera.cache_cy);
        metadata<<",\"render_passes\":[";
        for(unsigned i=0;i<ctx.render_passes.size();++i){const auto& p=ctx.render_passes[i];if(i)metadata<<',';metadata<<'['<<(p.autosort?1:0)<<','<<p.op_count<<','<<p.pt_count<<','<<p.tr_count<<','<<p.sorted_tr_count<<']';}
        metadata<<"],\"sorted_triangles\":[";
        for(unsigned i=0;i<ctx.sortedTriangles.size();++i){const auto& p=ctx.sortedTriangles[i];if(i)metadata<<',';metadata<<'['<<p.polyIndex<<','<<p.first<<','<<p.count<<']';}
        metadata<<"]}";const std::string json=metadata.str();
        static std::atomic<unsigned> nonce{};const auto host=std::chrono::system_clock::now().time_since_epoch().count();
        const auto path=directory_/("dc-geometry-"+std::to_string(host)+"-"+std::to_string(nonce.fetch_add(1))+"-"+std::to_string(camera.submit_sequence)+(camera.is_rtt?"-rtt.ssdcg":"-main.ssdcg"));
        std::error_code error;if(std::filesystem::exists(path,error)||error){enabled_=false;return false;}
        std::ofstream output(path,std::ios::binary);if(!output){enabled_=false;return false;}
        output.write("SSDCG001",8);word(output,uint32_t(json.size()));word(output,uint32_t(ctx.verts.size()));word(output,uint32_t(ctx.idx.size()));word(output,uint32_t(polygons));output.write(json.data(),std::streamsize(json.size()));
        for(const auto& vertex:ctx.verts){for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})scalar(output,value);output.write(reinterpret_cast<const char*>(vertex.col),4);}
        for(auto index:ctx.idx)word(output,uint32_t(index));
        const auto list=[&](const auto& params,uint32_t listId){for(const auto& p:params)for(uint32_t value:{listId,p.first,p.count,p.isp.full,p.tsp.full,p.tcw.full,p.pcw.full,p.tileclip})word(output,value);};
        list(ctx.global_param_op,0);list(ctx.global_param_pt,1);list(ctx.global_param_tr,2);
        output.close();if(!output){enabled_=false;std::filesystem::remove(path,error);return false;}
        last_sequence_[kind]=camera.submit_sequence;++records_;paths_.push_back(path);return true;
    }
};
inline CameraGeometryProbe& environment_camera_geometry_probe(){
    static const bool ui=[](){const char* flag=std::getenv("SILENT_SCOPE_DC_UI_PROBE");return flag&&std::strcmp(flag,"1")==0;}();
    static const auto limits=camera_geometry_probe_limits(ui,std::getenv("SILENT_SCOPE_DC_CAMERA_PROBE_END_SECONDS"));
    static CameraGeometryProbe probe([](){const char* directory=std::getenv("SILENT_SCOPE_DC_CAMERA_GEOMETRY_DIR");return directory?std::filesystem::path(directory):std::filesystem::path();}(),limits.records,limits.begin,limits.end);return probe;
}
}}
