// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_camera.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
namespace silent_scope::scene {
// Value copied into an immutable scene snapshot. A requested menu flag never
// proves a native camera was applied. An invalid lens requires original raster.
struct WiderViewState {
    bool requested{},applied{};
    double factor{1};
    NativeLens lens;
};
// Emulation-thread-only adapter for observed US UAD1.33 main DSP projection.
// The six output words are independent of the raw lens, cabinet aim and DSP2.
// Program guards and native side-plane equations come from captured main DSP
// disassembly:2012d..20153,20171..20176,20d69..20d82,20e2c..20e6b,21fd2..21fdb.
class WiderView {
public:
    static constexpr double MaxWideFactor=4; // Bounds only; factors above3 need native evidence.
    static constexpr std::array<uint32_t,6> Fields{0x28235,0x28237,0x2823a,0x2823b,0x2823c,0x2823d};
    static constexpr std::array<std::pair<uint32_t,uint64_t>,20> ProgramSignatures{{
        {0x20042,0x0f1600028263ULL},{0x20043,0x0f1700028220ULL},
        {0x20134,0x6c3e518da2afULL},{0x2013b,0x6ebea90bd330ULL},
        {0x2013c,0x6ebeb98daaadULL},{0x20146,0x6ebed00915bdULL},
        {0x2014a,0x6ebed8d97038ULL},{0x20150,0x6ebee0130a00ULL},
        {0x20153,0xaf010000001dULL},{0x21fd2,0x6e3e97597038ULL},
        {0x20171,0x6e3ea10bd110ULL},{0x20172,0x6e3eb18bd227ULL},
        {0x20175,0x6ebea90bd337ULL},{0x20176,0xaf0300000017ULL},
        {0x20d6d,0x6e3ed108a03cULL},{0x20d70,0x6e3edb88a037ULL},
        {0x20e2c,0x6e3ed008a024ULL},{0x20e44,0x6e3ee0801999ULL},
        {0x21fd3,0x6e3eaa930c0cULL},{0x21fd7,0x6e3eba930b15ULL}}};
    // Pinned ADSP21062 2M internal PM-to-DM alias layout, also used by SharcSram.
    // Program uploads normally use PM dispatch, but a DM write to either backing
    // word changes that instruction too and must invalidate a cached camera.
    static constexpr std::array<uint32_t,2> program_data_aliases(uint32_t address){
        const uint32_t offset=address&0x7fff,slot=offset>>12,base=(offset&0xfff)+(slot>>1)*0x3000;
        const uint32_t block=address<0x28000?0x20000:0x28000;
        return {block+base+(slot&1?0x2000:0),block+base+0x1000};
    }
    template<class Program> static bool recognized(Program program,int width,int height){
        if(width!=512||height!=384)return false;
        for(auto [address,value]:ProgramSignatures)if(program(address)!=value)return false;
        return true;
    }
    void request(bool on,double factor=3){requested_=on;desired_factor_=factor;}
    // Call from a tap ONLY for a genuine native incoming write. The writer used by
    // synchronize/restore must bypass intercept_write (internal_write() is true).
    // Invalid/partial writes pass through and suppress geometry until synchronized.
    uint32_t intercept_write(uint32_t address,uint32_t incoming,uint32_t mask,bool guard){
        if(internal_write_)return incoming;
        auto it=std::find(Fields.begin(),Fields.end(),address);
        if(it==Fields.end())return incoming;
        const size_t index=it-Fields.begin();
        // Any genuine pass-through native write supersedes ownership, even if
        // its bits happen to equal our previous output. Restore must not undo it.
        if(!active_){owned_[index]=false;return incoming;}
        if(!guard||mask!=UINT32_MAX){active_=false;owned_[index]=false;return incoming;}
        const float value=std::bit_cast<float>(incoming);
        if(!plausible(index,value)){active_=false;owned_[index]=false;return incoming;}
        stock_[index]=incoming;written_[index]=transform(index,value,active_factor_);owned_[index]=true;
        return written_[index];
    }
    // Native SDKs frequently rewrite identical camera words. Preserve the
    // existing proven cache only when the actual masked memory value stays
    // identical after interception. Any unknown guard or real change invalidates
    // it; this never substitutes a previous lens for changed/unknown hardware.
    uint32_t intercept_write_cached(uint32_t address,uint32_t incoming,uint32_t mask,bool guard,
                                    uint32_t old_word,WiderViewState& cached){
        const bool camera_field=std::find(Fields.begin(),Fields.end(),address)!=Fields.end();
        const auto result=intercept_write(address,incoming,mask,guard);
        if(camera_field&&!internal_write_&&(!guard||((old_word^result)&mask)!=0))cached={};
        return result;
    }
    bool internal_write()const{return internal_write_;}
    uint64_t restore_generation()const{return restore_generation_;}
    static constexpr bool setup_boundary(uint32_t pc,uint32_t address){
        return (pc==0x20153&&address==0x2823d)||(pc==0x20176&&address==0x28237);
    }
    // Apply/restore only at a proven final SDK camera store, with the pending
    // current word exposed by staged accessors. Arbitrary OSD/screen polls may
    // observe/request but cannot modify vertices already being projected.
    template<class Program,class Data,class Write>
    WiderViewState synchronize_at_setup(Program program,Data data,Write write,int width,int height,bool gameplay,uint32_t pc,uint32_t address){
        const bool code=recognized(program,width,height);
        if(!setup_boundary(pc,address)||!code)return frame_state(program,data,width,height,gameplay);
        const bool wanted=requested_&&std::isfinite(desired_factor_)&&desired_factor_>=1&&desired_factor_<=MaxWideFactor&&desired_factor_>1&&gameplay&&code;
        if(!wanted){restore(data,write);return frame_state(program,data,width,height,gameplay);}
        auto native=read_silent_scope_lens(program,data,width,height);
        std::array<uint32_t,6> source{};
        for(size_t i=0;i<Fields.size();++i){const auto current=uint32_t(data(Fields[i]));source[i]=owned_[i]&&current==written_[i]?stock_[i]:current;}
        if(!coherent(native,data,source)){restore(data,write);return frame_state(program,data,width,height,gameplay);}
        // No factor is ever applied to our own last output. Stock words retain
        // exact bit patterns for Original, including native depth-scale rewrites.
        internal_write_=true;
        for(size_t i=0;i<Fields.size();++i){stock_[i]=source[i];written_[i]=transform(i,std::bit_cast<float>(source[i]),desired_factor_);owned_[i]=true;write(Fields[i],written_[i]);}
        internal_write_=false;active_=true;active_factor_=desired_factor_;
        return frame_state(program,data,width,height,gameplay);
    }
    template<class Program,class Data>
    WiderViewState frame_state(Program program,Data data,int width,int height,bool gameplay)const{
        WiderViewState state{requested_,false,1,{}};
        if(!gameplay||!recognized(program,width,height))return state;
        auto native=read_silent_scope_lens(program,data,width,height);
        const bool owned=std::any_of(owned_.begin(),owned_.end(),[](bool value){return value;});
        if(!owned){std::array<uint32_t,6> source{};for(size_t i=0;i<Fields.size();++i)source[i]=data(Fields[i]);if(coherent(native,data,source))state.lens=native;return state;}
        if(!active_||!gameplay||!coherent(native,data,stock_))return state;
        for(size_t i=0;i<Fields.size();++i)if(!owned_[i]||data(Fields[i])!=written_[i])return state;
        native.fx/=active_factor_;native.fy/=active_factor_;
        state.applied=true;state.factor=active_factor_;state.lens=native;return state;
    }
private:
    bool requested_{true},active_{},internal_write_{};
    double desired_factor_{3},active_factor_{1};
    uint64_t restore_generation_{};
    std::array<uint32_t,6> stock_{},written_{};
    std::array<bool,6> owned_{};
    static bool plausible(size_t index,float value){
        return std::isfinite(value)&&(index<2?value>0&&value<131072:index==2||index==4?value>0&&value<16:value>=1&&value<257);
    }
    static uint32_t transform(size_t index,float value,double factor){
        const double result=index<2?value/factor:index==2||index==4?value*factor:1+(value-1)*factor*factor;
        return std::bit_cast<uint32_t>(float(result));
    }
    static bool near(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=2e-5*std::max({1.,std::abs(a),std::abs(b)});}
    template<class Data> static bool coherent(NativeLens lens,Data data,const std::array<uint32_t,6>& words){
        if(!lens.valid()||lens.fx!=443||lens.fy!=443||lens.cx!=256||lens.cy!=192)return false;
        std::array<float,6> value{};for(size_t i=0;i<value.size();++i){value[i]=std::bit_cast<float>(words[i]);if(!plausible(i,value[i]))return false;}
        const auto depth_scale=double(std::bit_cast<float>(uint32_t(data(0x2823e))));
        return depth_scale<0&&near(value[0]*-depth_scale,lens.fx*16)&&near(value[1]*-depth_scale,lens.fy*16)
            &&near(value[2],256/lens.fx)&&near(value[4],192/lens.fy)
            &&near(value[3],1+value[2]*value[2])&&near(value[5],1+value[4]*value[4]);
    }
    template<class Data,class Write> void restore(Data data,Write write){
        active_=false;internal_write_=true;bool wrote=false;
        for(size_t i=0;i<Fields.size();++i){if(owned_[i]&&data(Fields[i])==written_[i]){write(Fields[i],stock_[i]);wrote=true;}owned_[i]=false;}
        if(wrote)++restore_generation_;
        internal_write_=false;active_factor_=1;
    }
};
}
