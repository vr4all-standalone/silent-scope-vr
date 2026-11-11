// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "dc_lens.h"
namespace silent_scope { namespace dreamcast {
inline bool complete_main_lens(const SubmittedLens& v){
    return v.valid&&v.supported&&v.generation&&v.width==640&&v.height==480&&v.cx==320&&v.cy==240
        &&v.stock_fx==555&&v.stock_fy==555&&((v.factor==1&&v.fx==555&&v.fy==555)||(v.factor==3&&v.fx==185&&v.fy==185));
}
inline bool same_projection(const SubmittedLens& a,const SubmittedLens& b){
    return complete_main_lens(a)&&complete_main_lens(b)&&a.generation==b.generation&&a.request_revision==b.request_revision
        &&a.fx==b.fx&&a.fy==b.fy&&a.cx==b.cx&&a.cy==b.cy&&a.width==b.width&&a.height==b.height;
}
// Host-only bounded source-buffer provenance. An unknown overwrite removes
// previous ownership. Every byte of a queried range must be covered by one
// coherent lens; partial coverage, mixed projection and overflow fail closed.
// The caller must attest the projector's actual header/outputs. This ledger
// cannot manufacture provenance from a live SDK sample or a requested setting.
template<size_t Capacity=64> class ProjectionRangeLedger {
    struct Range{uint32_t begin{},end{};SubmittedLens lens;};
    std::array<Range,Capacity> ranges_{};size_t size_{};uint64_t generation_{};bool overflow_{};
public:
    void reset(uint64_t generation){size_=0;generation_=generation;overflow_=false;}
    size_t ranges()const{return size_;}
    bool overflowed()const{return overflow_;}
    // TA packets arrive in order. Avoid allocating/clearing a scratch range
    // array for every32-byte packet; equal adjacent ownership extends in place.
    bool append(uint32_t begin,uint32_t length,const SubmittedLens& value){
        if(overflow_||!length||uint64_t(begin)+length>UINT32_MAX)return false;
        if(size_&&ranges_[size_-1].end>begin)return write(begin,length,value);
        if(value.valid&&(!complete_main_lens(value)||value.generation!=generation_)){size_=0;return false;}
        if(!value.valid)return true;
        if(size_&&ranges_[size_-1].end==begin&&same_projection(ranges_[size_-1].lens,value)){ranges_[size_-1].end=begin+length;return true;}
        if(size_==Capacity){size_=0;overflow_=true;return false;}
        ranges_[size_++]={begin,begin+length,value};return true;
    }
    bool write(uint32_t begin,uint32_t length,const SubmittedLens& value){
        if(overflow_||!length||uint64_t(begin)+length>UINT32_MAX)return false;
        if(value.valid&&(!complete_main_lens(value)||value.generation!=generation_)){size_=0;return false;}
        const uint32_t end=begin+length;
        std::array<Range,Capacity*2+1> next{};size_t count=0;
        for(size_t i=0;i<size_;++i){const auto& old=ranges_[i];
            if(old.end<=begin||old.begin>=end)next[count++]=old;
            else{
                if(old.begin<begin)next[count++]={old.begin,begin,old.lens};
                if(old.end>end)next[count++]={end,old.end,old.lens};
            }
        }
        if(value.valid)next[count++]={begin,end,value};
        std::sort(next.begin(),next.begin()+count,[](const Range& a,const Range& b){return a.begin<b.begin;});
        size_t merged=0;
        for(size_t i=0;i<count;++i){
            if(merged&&next[merged-1].end==next[i].begin&&same_projection(next[merged-1].lens,next[i].lens))next[merged-1].end=next[i].end;
            else next[merged++]=next[i];
        }
        if(merged>Capacity){size_=0;overflow_=true;return false;}
        size_=merged;std::copy_n(next.begin(),size_,ranges_.begin());return true;
    }
    SubmittedLens lookup(uint32_t begin,uint32_t length)const{
        if(overflow_||!length||uint64_t(begin)+length>UINT32_MAX)return {};
        const uint32_t end=begin+length;uint32_t cursor=begin;SubmittedLens value;
        for(size_t i=0;i<size_;++i){const auto& range=ranges_[i];
            if(range.end<=cursor)continue;
            if(range.begin>cursor)return {};
            if(!value.valid)value=range.lens;
            else if(!same_projection(value,range.lens))return {};
            cursor=std::min(range.end,end);if(cursor==end)return value;
        }
        return {};
    }
};
// The final TA identity is bound only after the actual owned source was joined
// to that TA buffer. No live SDK/current-frame/request state is consulted here.
inline SubmittedLens publish_owned_lens(const SubmittedLens& owned,uint64_t generation,uint64_t submission,
        unsigned width,unsigned height,bool rtt,bool ownership_proven){
    if(!ownership_proven||rtt||!submission||width!=640||height!=480||owned.generation!=generation||!complete_main_lens(owned))return {};
    auto lens=owned;lens.submit_sequence=submission;return lens;
}
// Feed only actual first32-byte polygon vertex packets identified by native TA
// FSM. Header packets and64-byte second halves never enter this accumulator.
// Only separately proved native UI may bypass ownership. Constant-q world
// planes participate too; unknown/mixed provenance invalidates the main view.
class NativeLensAccumulator {
    struct Point{float x{},y{},q{};SubmittedLens lens;};
    std::array<Point,2> previous_{};unsigned vertices_{},triangles_{};bool rejected_{};SubmittedLens lens_;
public:
    void reset(){vertices_=triangles_=0;rejected_=false;lens_={};}
    void break_strip(){vertices_=0;}
    void reject(){rejected_=true;}
    bool rejected()const{return rejected_;}
    unsigned triangles()const{return triangles_;}
    void vertex(float x,float y,float q,const SubmittedLens& lens,bool end_strip,bool proven_ui_triangle=false){
        const Point current{x,y,q,lens};
        if(vertices_>=2){
            const auto& a=previous_[0];const auto& b=previous_[1];
            const double area=double(b.x-a.x)*(y-a.y)-double(b.y-a.y)*(x-a.x);
            const float low=std::min({a.q,b.q,q});
            if(std::isfinite(area)&&std::abs(area)>1e-6&&std::isfinite(a.q)&&std::isfinite(b.q)&&std::isfinite(q)
                &&std::isfinite(low)&&low>0&&!proven_ui_triangle){
                if(!same_projection(a.lens,b.lens)||!same_projection(a.lens,lens)||(lens_.valid&&!same_projection(lens_,lens)))rejected_=true;
                else{lens_=lens;++triangles_;}
            }
        }
        previous_[0]=previous_[1];previous_[1]=current;++vertices_;
        if(end_strip)vertices_=0;
    }
    SubmittedLens owned()const{return !rejected_&&triangles_>=4?lens_:SubmittedLens{};}
};
}}
