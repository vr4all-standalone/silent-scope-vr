// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Bounded desktop native-mouse experiment. This is not a VR adapter. RAM is
// read on the emulation thread; no game memory is written or dumped to disk.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace silent_scope { namespace dc_input_diagnostic {
// Native active-low mouse bit indices: core/input/mouse.h Mouse::Button and
// mouse.cpp Mouse::setButton. No synthetic native "Start" button is invented.
constexpr std::uint8_t Left=1u<<2,Right=1u<<1,Middle=1u<<3;
constexpr std::size_t MaxRamBytes=16u*1024*1024,MaxCandidates=65536,MaxResults=128;
constexpr std::size_t RotationPairOffset=0x1b0164+0x69c+0x60;
static_assert(RotationPairOffset==0x1b0860,"Title rotation field address arithmetic");
struct Sample {bool owned{};std::uint8_t pressed{};int x{},y{};double elapsed{};};
inline int parse_step(const char* value)noexcept {
    if(!value)return 32;
    const std::string_view text(value);
    return text=="16"?16:text=="32"?32:text=="48"?48:0;
}
class Script {
public:
    explicit Script(bool enabled=false,int counts=32):enabled_(enabled&&(counts==16||counts==32||counts==48)),counts_(counts){}
    Sample poll(std::string_view product,double time) noexcept {
        if(!enabled_||cancelled_)return {};
        if(product!="T9507N"){if(started_)cancelled_=true;return {};}
        if(!std::isfinite(time)||time<0)return {};
        if(!started_){first_=last_=time;started_=true;}
        if(time<last_)return {true,0,0,0,last_-first_};
        last_=time;const double t=time-first_;
        Sample result{true,0,0,0,t};
        if(t>=64)return result;
        if((t<15||(t>=21&&t<28))&&time<first_+std::floor(t)+.1)result.pressed=Left;
        if(t>=20&&time<first_+20.1)result.pressed=Middle;
        for(unsigned i=0;i<8;++i) {
            const double at=42+2*i;
            if(t>=at&&!done_[i]) {
                done_[i]=true; // Never defer a missed impulse into another phase.
                if(time<first_+at+.1) {
                    const int delta=i%2?-counts_:counts_;
                    if(i<4)result.x=delta;else result.y=delta;
                }
            }
        }
        for(unsigned i=0;i<3;++i) {
            const double at=58+2*i;
            if(t>=at&&time<first_+at+.1)result.pressed=i==0?Right:i==1?Middle:Left;
        }
        return result;
    }
private:
    bool enabled_{},started_{},cancelled_{};int counts_{32};double first_{},last_{};std::array<bool,8> done_{};
};
enum class Kind {Float,Signed,Signed16Low,Signed16High};
inline double numeric(std::uint32_t bits,Kind kind) noexcept {
    if(kind==Kind::Float){float value;std::memcpy(&value,&bits,4);return value;}
    if(kind==Kind::Signed16Low||kind==Kind::Signed16High) {
        const std::uint16_t half=std::uint16_t(bits>>(kind==Kind::Signed16High?16:0));
        std::int16_t value;std::memcpy(&value,&half,2);return value;
    }
    std::int32_t value;std::memcpy(&value,&bits,4);return value;
}
inline bool plausible(double value,Kind kind) noexcept {
    return std::isfinite(value)&&std::abs(value)<=65536
        &&(kind!=Kind::Float||value==0||std::abs(value)>=1e-8);
}
inline bool reverses(const std::array<double,5>& v,Kind kind) noexcept {
    for(double x:v)if(!plausible(x,kind))return false;
    const double a=v[1]-v[0],b=v[2]-v[1],c=v[3]-v[2],d=v[4]-v[3];
    const double floor=kind==Kind::Float?1e-5:.5;
    if(std::min({std::abs(a),std::abs(b),std::abs(c),std::abs(d)})<floor)return false;
    if(a*c<=0||b*d<=0||a*b>=0||c*d>=0)return false;
    const double tolerance=std::max(kind==Kind::Float?1e-5:0.,
        std::max({std::abs(a),std::abs(b),std::abs(c),std::abs(d)})*.01);
    return std::abs(v[2]-v[0])<=tolerance&&std::abs(v[4]-v[2])<=tolerance
        &&std::abs(a-c)<=tolerance&&std::abs(b-d)<=tolerance;
}
struct Candidate {std::uint32_t offset{};Kind kind{};std::array<double,5> values{};double error{};};
class AxisScan {
public:
    bool baseline(const void* memory,std::size_t bytes) {
        if(!memory||bytes==0||bytes>MaxRamBytes||bytes%4)return false;
        base_.resize(bytes/4);std::memcpy(base_.data(),memory,bytes);
        records_.clear();observed_=0;dropped_=0;return true;
    }
    bool observe(const void* memory,std::size_t bytes,unsigned phase) {
        if(!memory||base_.empty()||bytes!=base_.size()*4||phase!=observed_||phase>=4)return false;
        const auto* data=static_cast<const unsigned char*>(memory);
        if(phase==0) {
            for(std::size_t i=0;i<base_.size();++i) {
                std::uint32_t value;std::memcpy(&value,data+i*4,4);
                if(value==base_[i])continue;
                bool eligible=false;
                for(Kind kind:{Kind::Float,Kind::Signed,Kind::Signed16Low,Kind::Signed16High})
                    eligible|=plausible(numeric(value,kind),kind)&&plausible(numeric(base_[i],kind),kind);
                if(!eligible)continue;
                if(records_.size()==MaxCandidates){++dropped_;continue;}
                records_.push_back({std::uint32_t(i*4),{base_[i],value,0,0,0}});
            }
        } else {
            for(auto& record:records_)std::memcpy(&record.bits[phase+1],data+record.offset,4);
        }
        ++observed_;return true;
    }
    std::vector<Candidate> results(std::size_t* matches=nullptr) const {
        if(matches)*matches=0;
        std::vector<Candidate> result;if(observed_!=4||dropped_)return result;
        // Consider every retained address, then keep the best bounded results.
        // Lower-address audio/geometry churn cannot consume the output budget.
        const auto better=[](const Candidate& a,const Candidate& b){
            return a.error!=b.error?a.error<b.error:a.offset!=b.offset?a.offset<b.offset:a.kind<b.kind;};
        result.reserve(MaxResults);
        for(const auto& record:records_)for(Kind kind:{Kind::Float,Kind::Signed,Kind::Signed16Low,Kind::Signed16High}) {
            Candidate candidate{record.offset+(kind==Kind::Signed16High?2u:0u),kind,{}};
            for(unsigned i=0;i<5;++i)candidate.values[i]=numeric(record.bits[i],kind);
            if(!reverses(candidate.values,kind))continue;
            if(matches)++*matches;
            const auto& v=candidate.values;
            const double a=v[1]-v[0],b=v[2]-v[1],c=v[3]-v[2],d=v[4]-v[3];
            candidate.error=std::max({std::abs(v[2]-v[0]),std::abs(v[4]-v[2]),std::abs(a-c),std::abs(b-d)})
                /std::max({std::abs(a),std::abs(b),std::abs(c),std::abs(d)});
            if(result.size()<MaxResults){result.push_back(candidate);std::push_heap(result.begin(),result.end(),better);}
            else if(better(candidate,result.front())) {
                std::pop_heap(result.begin(),result.end(),better);result.back()=candidate;
                std::push_heap(result.begin(),result.end(),better);
            }
            // A matching float/int32 word otherwise also yields misleading
            // quantized halfword aliases. Retain its whole-word interpretation;
            // try independent signed16 halves when neither whole-word type fits.
            if(kind==Kind::Float||kind==Kind::Signed)break;
        }
        std::sort(result.begin(),result.end(),better);
        return result;
    }
    std::size_t candidate_count() const noexcept{return records_.size();}
    std::size_t dropped() const noexcept{return dropped_;}
    void release() {std::vector<std::uint32_t>().swap(base_);std::vector<Record>().swap(records_);}
private:
    struct Record {std::uint32_t offset;std::array<std::uint32_t,5> bits;};
    std::vector<std::uint32_t> base_;std::vector<Record> records_;unsigned observed_{};std::size_t dropped_{};
};
inline void source_watch(const void* ram,std::size_t bytes,unsigned axis,int phase,double native,double elapsed) {
    if(!ram||bytes<0x272000)return;
    const auto* data=static_cast<const unsigned char*>(ram);
    const auto word=[&](std::size_t at){std::uint16_t value;std::memcpy(&value,data+at,2);return value;};
    // Owned executable 3cf1879f...: read-pair at b4f2/b4f4 followed by the
    // b4f6 call; b4de adds the title base +0x69c, then b4ec adds +0x60.
    std::uint32_t base;std::memcpy(&base,data+0x01b5a8,4);
    const bool guard=word(0x01b4de)==0x332c&&word(0x01b4ec)==0x7560
                    &&word(0x01b4f2)==0x6551&&word(0x01b4f4)==0x8531&&word(0x01b4f6)==0x420b
                    &&word(0x01b5a2)==0x69c&&base==0x8c1b0164
                    &&word(0x073e6e)==0x0e15&&word(0x073e7c)==0x0e15
                    &&word(0x073ee0)==0x06fe&&word(0x073ee2)==0x06fc
                    &&word(0x073ee4)==0x157c&&word(0x073ee6)==0xea84
                    &&word(0x073ee8)==0x10cc&&word(0x073eea)==0xef34;
    if(!guard){
        static bool detail_logged=false;
        if(!detail_logged){
            std::fprintf(stderr,"DC_MOUSE_DIAG guard_actual b4de=%04x b4ec=%04x b4f2=%04x b4f4=%04x b4f6=%04x b5a2=%04x base=%08x expected=332c,7560,6551,8531,420b,069c,8c1b0164\n",
                unsigned(word(0x01b4de)),unsigned(word(0x01b4ec)),unsigned(word(0x01b4f2)),unsigned(word(0x01b4f4)),unsigned(word(0x01b4f6)),unsigned(word(0x01b5a2)),base);
            std::fprintf(stderr,"DC_MOUSE_DIAG guard_writer_actual e6e=%04x e7c=%04x ee0=%04x ee2=%04x ee4=%04x ee6=%04x ee8=%04x eea=%04x expected=0e15,0e15,06fe,06fc,157c,ea84,10cc,ef34\n",
                unsigned(word(0x073e6e)),unsigned(word(0x073e7c)),unsigned(word(0x073ee0)),unsigned(word(0x073ee2)),unsigned(word(0x073ee4)),unsigned(word(0x073ee6)),unsigned(word(0x073ee8)),unsigned(word(0x073eea)));
            detail_logged=true;
        }
        std::fprintf(stderr,"DC_MOUSE_DIAG source_watch axis=%c phase=%d code_guard=false\n",axis?'y':'x',phase);return;
    }
    std::int16_t a,b;std::memcpy(&a,data+RotationPairOffset,2);std::memcpy(&b,data+RotationPairOffset+2,2);
    float sx,sy,size;std::memcpy(&sx,data+0x0ace5c,4);std::memcpy(&sy,data+0x0ace60,4);std::memcpy(&size,data+0x0ace64,4);
    std::fprintf(stderr,"DC_MOUSE_DIAG source_watch axis=%c phase=%d native_seconds=%.6f t=%.6f code_guard=true rotation_arg_r5_8c1b0860=%d rotation_arg_r4_8c1b0862=%d scope_ui_x_8c0ace5c=%.9g scope_ui_y_8c0ace60=%.9g scope_ui_size_8c0ace64=%.9g unverified=true\n",
        axis?'y':'x',phase,native,elapsed,int(a),int(b),double(sx),double(sy),double(size));
    const auto integer=[&](std::size_t at){std::uint32_t value;std::memcpy(&value,data+at,4);return value;};
    const auto scalar=[&](std::size_t at){float value;std::memcpy(&value,data+at,4);return double(value);};
    const unsigned speed=integer(0x26fe1c);
    std::fprintf(stderr,"DC_MOUSE_DIAG gain_context native_seconds=%.6f t=%.6f axis=%c phase=%d speed_index_26fe1c=%u reverse_y_26fe18=%u reverse_x_bit_26fdc7=%u mode_271ffc=%u scope_branch_1b013c=%u input_sign_1b015c=%d table_factor_known=%s table_factor=%.9g last_increment_x_c7aa0=%.9g last_increment_y_c7aa4=%.9g unverified=true\n",
        native,elapsed,axis?'y':'x',phase,speed,integer(0x26fe18),unsigned((data[0x26fdc7]&0x10)!=0),integer(0x271ffc),integer(0x1b013c),int(std::int8_t(data[0x1b015c])),speed<3?"true":"false",speed<3?scalar(0x0c7a88+4*speed):0.,scalar(0x0c7aa0),scalar(0x0c7aa4));
}
inline int environment_step()noexcept {
    static const int value=parse_step(std::getenv("SILENT_SCOPE_DC_MOUSE_DIAG_COUNTS"));return value;
}
inline bool environment_enabled() noexcept {
    static const bool enabled=[] {const char* value=std::getenv("SILENT_SCOPE_DC_MOUSE_DIAG");
        return value&&std::string_view(value)=="middle-steps"&&environment_step()!=0;}();
    return enabled;
}
inline void environment_poll(std::string_view product,double native_seconds,const void* ram,std::size_t bytes,
                             std::uint8_t& buttons,int& x,int& y,int& wheel) noexcept {
    static bool resource_failed=false;
    if(!environment_enabled()||resource_failed)return;
    try {
        struct Runtime {
            Script script{true,environment_step()};AxisScan scan;unsigned axis{},observed{},logs{};
            bool baseline{},failed{},finished{},logged_start{};std::uint8_t last_buttons{};
        };
        static Runtime state;
        const Sample value=state.script.poll(product,native_seconds);if(!value.owned)return;
        if(!state.logged_start) {
            std::fprintf(stderr,"DC_MOUSE_DIAG begin schema=3 product=T9507N native_seconds=%.6f script=middle-steps step_counts=%d ram_bytes=%zu\n",native_seconds,environment_step(),bytes);
            state.logged_start=true;
        }
        const double start=state.axis==0?42:50;
        if(!state.finished&&!state.failed) {
            if(!state.baseline&&value.elapsed>=start-.5) {
                if(value.elapsed>=start+.1||!state.scan.baseline(ram,bytes))state.failed=true;
                else {state.baseline=true;source_watch(ram,bytes,state.axis,-1,native_seconds,value.elapsed);}
            }
            if(state.baseline&&value.elapsed>=start+2*state.observed+.25) {
                if(value.elapsed>=start+2*state.observed+.5||!state.scan.observe(ram,bytes,state.observed))state.failed=true;
                else {
                    source_watch(ram,bytes,state.axis,int(state.observed),native_seconds,value.elapsed);
                    if(++state.observed==4) {
                    std::size_t matches=0;const auto results=state.scan.results(&matches);
                    std::fprintf(stderr,"DC_MOUSE_DIAG axis=%c native_seconds=%.6f t=%.6f status=%s candidates=%zu dropped_initial=%zu matches=%zu result_limit=%zu emitted=%zu\n",
                        state.axis?'y':'x',native_seconds,value.elapsed,state.scan.dropped()?"incomplete_overflow":"complete_unverified",
                        state.scan.candidate_count(),state.scan.dropped(),matches,MaxResults,results.size());
                    for(const auto& result:results)std::fprintf(stderr,
                        "DC_MOUSE_DIAG candidate axis=%c offset=0x%06x type=%s values=[%.9g,%.9g,%.9g,%.9g,%.9g] normalized_error=%.9g unverified=true\n",
                        state.axis?'y':'x',result.offset,result.kind==Kind::Float?"float32":result.kind==Kind::Signed?"signed32":"signed16",
                        result.values[0],result.values[1],result.values[2],result.values[3],result.values[4],result.error);
                    if(state.axis==0){state.axis=1;state.baseline=false;state.observed=0;}
                    else {state.finished=true;state.scan.release();}
                    }
                }
            }
            if(state.failed){state.scan.release();std::fputs("DC_MOUSE_DIAG RAM_sampling_failed_or_missed_deadline=true\n",stderr);}
        }
        // This explicit experiment owns the three real native mouse buttons and
        // relative axes. It bypasses external host motion only while opted in.
        buttons=std::uint8_t((buttons|(Left|Right|Middle))&~value.pressed);
        x=value.x;y=value.y;wheel=0;
        if(state.logs<128&&(value.pressed!=state.last_buttons||x||y||state.logs==0)) {
            std::fprintf(stderr,"DC_MOUSE_DIAG input native_seconds=%.6f t=%.6f pressed_native_bits=%u relative_x=%d relative_y=%d wheel=0\n",
                native_seconds,value.elapsed,unsigned(value.pressed),x,y);++state.logs;
        }
        state.last_buttons=value.pressed;
    } catch(...) {
        // No exception crosses native DMA. A resource failure is visible and
        // leaves this call's already-acquired native input intact.
        resource_failed=true;std::fputs("DC_MOUSE_DIAG resource_failure=true\n",stderr);
    }
}
}} // namespace silent_scope::dc_input_diagnostic
