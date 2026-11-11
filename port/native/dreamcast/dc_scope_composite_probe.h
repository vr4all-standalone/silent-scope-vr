// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_scope_composite.h"
// Opt-in desktop diagnostics only. Matching the RTT address identifies a
// candidate, not permission to suppress any draw. No context/RAM/GL mutation.
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <mutex>
#include <string>
#include <algorithm>
#include <type_traits>
#include <vector>

namespace silent_scope::dc_scope {
class CompositeProbe {
public:
    explicit CompositeProbe(std::filesystem::path directory,double begin_seconds=0):directory_(std::move(directory)),begin_(begin_seconds) {
        std::error_code error;
        enabled_=directory_.is_absolute()&&std::filesystem::is_directory(directory_,error)&&!error;
        static std::atomic<unsigned> nonce{};
        path_=directory_/("dc-scope-polygons-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+
                          "-"+std::to_string(nonce.fetch_add(1))+".jsonl");
        if(!std::isfinite(begin_)||begin_<0||begin_>120)begin_=0;
    }
    const std::filesystem::path& path()const{return path_;}
    unsigned records()const{return records_;}
    template<class Context> bool record(const std::string& product,const Context& context,double now)noexcept{
        try{
            std::lock_guard<std::mutex> lock(mutex_);
            if(!enabled_||records_>=8||product!="T9507N"||context.isRTT||!std::isfinite(now))return false;
            if(!began_){start_=now;began_=true;}
            if(now<start_||now-start_>120){enabled_=false;return false;}
            if(now-start_<begin_||(last_>=0&&now-last_<1))return false;
            const auto candidate=[](const auto& p){return p.pcw.Texture&&((p.tcw.TexAddr*8u)&0x7fffffu)==0x52c000u;};
            if(context.global_param_op.size()+context.global_param_pt.size()+context.global_param_tr.size()>16384)return false;
            using Polygon=typename std::decay_t<decltype(context.global_param_op)>::value_type;
            struct Choice{const char* list;std::size_t index;const Polygon* polygon;double area;PolygonStorage storage;};
            std::vector<Choice> selected;
            unsigned candidates=0;
            const auto collect=[&](const char* name,const auto& list){
                for(std::size_t pi=0;pi<list.size();++pi){const auto& p=list[pi];if(!candidate(p))continue;++candidates;
                    const auto storage=std::string_view(name)=="translucent"?translucent_storage(context,pi):PolygonStorage::Indices;
                    double minx=1e100,miny=1e100,maxx=-1e100,maxy=-1e100;
                    for(unsigned j=0;j<p.count&&j<64;++j){
                        const uint64_t at=uint64_t(p.first)+j;uint64_t index=at;
                        if(storage==PolygonStorage::Unknown)continue;
                        if(storage==PolygonStorage::Indices){if(at>=context.idx.size())continue;index=context.idx[at];}
                        if(index>=context.verts.size())continue;
                        const auto& v=context.verts[index];if(!std::isfinite(v.x)||!std::isfinite(v.y))continue;
                        minx=std::min(minx,double(v.x));maxx=std::max(maxx,double(v.x));
                        miny=std::min(miny,double(v.y));maxy=std::max(maxy,double(v.y));
                    }
                    const double area=std::max(0.,std::min(maxx,double(context.framebufferWidth))-std::max(0.,minx))
                        *std::max(0.,std::min(maxy,double(context.framebufferHeight))-std::max(0.,miny));
                    selected.push_back({name,pi,&p,area,storage});
                    std::stable_sort(selected.begin(),selected.end(),[](const Choice& a,const Choice& b){return a.area>b.area;});
                    if(selected.size()>16)selected.resize(16);
                }
            };
            collect("opaque",context.global_param_op);collect("punch",context.global_param_pt);collect("translucent",context.global_param_tr);
            if(!candidates)return false;
            std::ofstream out(path_,std::ios::app);if(!out){enabled_=false;return false;}
            out.imbue(std::locale::classic());out<<std::setprecision(12);
            out<<"{\"identity\":\"candidate_only_not_verified\",\"sequence\":"<<records_+1
                <<",\"host_seconds\":"<<now<<",\"generation\":"<<context.dcCamera.generation
                <<",\"submit_sequence\":"<<context.dcCamera.submit_sequence
                <<",\"main_destination\":"<<(context.fb_W_SOF1&0x7fffff)
                <<",\"width\":"<<context.framebufferWidth<<",\"height\":"<<context.framebufferHeight
                <<",\"candidate_count\":"<<candidates<<",\"selection_order\":\"visible_area_desc\",\"candidates_truncated\":"<<(candidates>16?"true":"false")<<",\"polygons\":[";
            unsigned emitted=0;
            for(const auto& choice:selected){
                    const char* name=choice.list;const auto pi=choice.index;const auto& p=*choice.polygon;
                    if(emitted++)out<<',';
                    out<<"{\"list\":\""<<name<<"\",\"poly_index\":"<<pi<<",\"first\":"<<p.first<<",\"count\":"<<p.count
                        <<",\"storage\":\""<<(choice.storage==PolygonStorage::Vertices?"vertices":choice.storage==PolygonStorage::Indices?"indices":"unknown")<<'"'
                        <<",\"texture_address\":"<<((p.tcw.TexAddr*8u)&0x7fffffu)
                        <<",\"tcw\":"<<p.tcw.full<<",\"tsp\":"<<p.tsp.full<<",\"pcw\":"<<p.pcw.full<<",\"isp\":"<<p.isp.full
                        <<",\"tileclip\":"<<p.tileclip<<",\"texture_width\":"<<(8u<<p.tsp.TexU)<<",\"texture_height\":"<<(8u<<p.tsp.TexV)
                        <<",\"scan_order\":"<<p.tcw.ScanOrder<<",\"stride_select\":"<<p.tcw.StrideSel<<",\"pixel_format\":"<<p.tcw.PixelFmt
                        <<",\"indices_truncated\":"<<(p.count>64?"true":"false")<<",\"vertices\":[";
                    for(unsigned j=0;j<p.count&&j<64;++j){if(j)out<<',';
                        const uint64_t position=uint64_t(p.first)+j;
                        if(choice.storage==PolygonStorage::Unknown){out<<"{\"valid\":false}";continue;}
                        uint64_t index=position;
                        if(choice.storage==PolygonStorage::Indices){if(position>=context.idx.size()){out<<"{\"valid\":false}";continue;}index=context.idx[position];}
                        if(index>=context.verts.size()){out<<"{\"valid\":false,\"index\":"<<index<<'}';continue;}
                        const auto& v=context.verts[index];const bool valid=std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v);
                        out<<"{\"valid\":"<<(valid?"true":"false")<<",\"index\":"<<index;
                        if(valid)out<<",\"x\":"<<v.x<<",\"y\":"<<v.y<<",\"q\":"<<v.z<<",\"u\":"<<v.u<<",\"v\":"<<v.v;
                        out<<",\"rgba\":["<<unsigned(v.col[0])<<','<<unsigned(v.col[1])<<','<<unsigned(v.col[2])<<','<<unsigned(v.col[3])<<"]}";
                    }
                    out<<"]}";
            }
            out<<"]}\n";out.close();if(!out){enabled_=false;return false;}last_=now;++records_;return true;
        }catch(...){enabled_=false;return false;}
    }
private:
    std::filesystem::path directory_,path_;
    bool enabled_{},began_{};
    unsigned records_{};
    double last_{-1},start_{},begin_{};
    std::mutex mutex_;
};
inline CompositeProbe& environment_composite_probe(){
    static CompositeProbe probe([]{const char* path=std::getenv("SILENT_SCOPE_DC_SCOPE_PROBE_DIR");return std::filesystem::path(path?path:"");}(),
        []{const char* begin=std::getenv("SILENT_SCOPE_DC_PROBE_BEGIN_SECONDS");if(!begin||!*begin)return 0.;char* end{};const double value=std::strtod(begin,&end);return end&&!*end?value:0.;}());
    return probe;
}
}
