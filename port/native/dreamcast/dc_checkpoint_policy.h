// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>
#include <algorithm>
namespace silent_scope { namespace dc_checkpoint {
enum class Mode:unsigned {None,Save,Load};
constexpr std::size_t HeaderBytes=80,MaxStateBytes=64u*1024*1024;
inline bool trim_payload(std::vector<std::uint8_t>& payload,std::size_t written) {
    if(payload.size()>MaxStateBytes||written<8||written>payload.size())return false;
    payload.resize(written);return true;
}
struct ByteDiff {std::size_t offset{};std::uint8_t saved{},loaded{};};
struct DiffRange {std::size_t offset{},length{};};
struct PayloadDiff {std::size_t different{},range_count{};std::vector<ByteDiff> bytes;std::vector<DiffRange> ranges;};
inline PayloadDiff compare_payloads(const std::vector<std::uint8_t>& saved,const std::vector<std::uint8_t>& loaded) {
    PayloadDiff out;const std::size_t common=std::min(saved.size(),loaded.size()),total=std::max(saved.size(),loaded.size());
    bool in_range=false;std::size_t start=0;
    const auto close=[&](std::size_t end){if(!in_range)return;++out.range_count;
        if(out.ranges.size()<64)out.ranges.push_back({start,end-start});
        in_range=false;};
    for(std::size_t i=0;i<total;++i) {
        const bool different=i>=common||saved[i]!=loaded[i];
        if(!different){close(i);continue;}
        ++out.different;if(!in_range){in_range=true;start=i;}
        if(i<common&&out.bytes.size()<64)out.bytes.push_back({i,saved[i],loaded[i]});
    }
    close(total);return out;
}
inline Mode parse_mode(const char* value)noexcept {
    if(!value)return Mode::None;
    return std::string_view(value)=="save40"?Mode::Save:std::string_view(value)=="load30"?Mode::Load:Mode::None;
}
class Policy {
    Mode mode_;bool requested_{};std::atomic<unsigned> pending_{};
public:
    explicit Policy(Mode mode=Mode::None):mode_(mode){}
    void observe(std::string_view product,double native)noexcept {
        if(mode_==Mode::None||requested_||product!="T9507N"||!std::isfinite(native))return;
        if(native<(mode_==Mode::Save?40.:30.))return;
        requested_=true;pending_.store(unsigned(mode_),std::memory_order_release);
    }
    Mode take()noexcept{return Mode(pending_.exchange(0,std::memory_order_acq_rel));}
};
struct Header {
    std::uint32_t version{},ram_bytes{};std::uint64_t bytes{},cycles{};
    std::array<std::uint8_t,16> payload_md5{},ram_md5{};
};
inline std::array<std::uint8_t,HeaderBytes> encode(const Header& value)noexcept {
    std::array<std::uint8_t,HeaderBytes> out{};std::size_t at=0;
    const auto number=[&](std::uint64_t v,unsigned count){for(unsigned i=0;i<count;++i){out[at++]=std::uint8_t(v);v>>=8;}};
    for(char c:std::string_view("SSDCST01"))out[at++]=std::uint8_t(c);
    number(1,4);number(value.version,4);number(value.bytes,8);number(value.cycles,8);number(value.ram_bytes,4);
    for(char c:std::array<char,8>{{'T','9','5','0','7','N',0,0}})out[at++]=std::uint8_t(c);
    for(auto b:value.payload_md5)out[at++]=b;
    for(auto b:value.ram_md5)out[at++]=b;
    number(0,4);return out;
}
inline bool decode(const std::array<std::uint8_t,HeaderBytes>& in,Header& value)noexcept {
    std::size_t at=0;
    const auto number=[&](unsigned count){std::uint64_t v=0;for(unsigned i=0;i<count;++i)v|=std::uint64_t(in[at++])<<(8*i);return v;};
    for(char c:std::string_view("SSDCST01"))if(in[at++]!=std::uint8_t(c))return false;
    if(number(4)!=1)return false;
    value.version=std::uint32_t(number(4));value.bytes=number(8);value.cycles=number(8);value.ram_bytes=std::uint32_t(number(4));
    for(char c:std::array<char,8>{{'T','9','5','0','7','N',0,0}})if(in[at++]!=std::uint8_t(c))return false;
    for(auto& b:value.payload_md5)b=in[at++];
    for(auto& b:value.ram_md5)b=in[at++];
    return number(4)==0&&value.version!=0&&value.bytes>=8&&value.bytes<=MaxStateBytes
        &&value.ram_bytes==16u*1024*1024&&value.cycles!=0;
}
// This rule is intentionally pinned to the independently attributed seed002;
// another native state/layout requires a new audit, not a guessed mask.
namespace Seed002 {
constexpr std::size_t PayloadBytes=27719372,Sh4=10896192,Scif=10904860,
    Smr=10904820,Brr=10904824,Context=27713115,Pointer=27713491;
constexpr std::uint64_t Cycles=8002667008ull,SavedPointer=0x7ff67c2f18a0ull;
constexpr std::array<std::uint8_t,16> PayloadMd5{{0xd1,0x95,0x8d,0xb4,0x73,0x6c,0xc5,0x12,0xb6,0x91,0x66,0x0c,0xde,0x61,0x99,0xc9}};
constexpr std::array<std::uint8_t,16> RamMd5{{0xff,0x47,0x8e,0xc3,0xe5,0x16,0xef,0xb6,0x77,0xfc,0xb5,0x28,0x1a,0xc0,0xf1,0x3b}};
}
inline std::uint64_t get64(const std::vector<std::uint8_t>& bytes,std::size_t at) {
    std::uint64_t out=0;for(unsigned i=0;i<8;++i)out|=std::uint64_t(bytes.at(at+i))<<(8*i);return out;
}
inline std::uint32_t get32(const std::vector<std::uint8_t>& bytes,std::size_t at) {
    std::uint32_t out=0;for(unsigned i=0;i<4;++i)out|=std::uint32_t(bytes.at(at+i))<<(8*i);return out;
}
inline void put32(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i){bytes.at(at+i)=std::uint8_t(value);value>>=8;}
}
inline void put64(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint64_t value) {
    for(unsigned i=0;i<8;++i){bytes.at(at+i)=std::uint8_t(value);value>>=8;}
}
inline std::uint32_t scif_period(std::uint32_t smr,std::uint32_t brr,std::uint32_t clock)noexcept {
    if(brr>255)return 0;
    const std::uint32_t bauds=clock/4/(brr+1)/32/(1u<<((smr&3)*2));
    if(!bauds)return 0;
    const std::uint32_t frame=10-((smr>>6)&1)+((smr>>5)&1)+((smr>>3)&1);
    return frame*(clock/bauds);
}
struct PostloadFacts {
    std::uint64_t cycles{},live_pointer{};std::uint32_t version{},clock{},live_smr{},live_brr{};
    std::size_t read_sh4{},write_sh4{},read_context{},write_context{},context_bytes{},pointer_relative{},pointer_bytes{};
    bool handler_verified{};
};
struct PostloadResult {bool equal{};const char* reason{};};
inline PostloadResult canonical_postload(std::vector<std::uint8_t>& expected,std::vector<std::uint8_t>& loaded,const PostloadFacts& facts) {
    if(expected.size()!=Seed002::PayloadBytes||loaded.size()!=Seed002::PayloadBytes
       ||facts.version!=854||facts.cycles!=Seed002::Cycles||facts.clock!=200000000)
        return {false,"unsupported_seed_size_version_clock"};
    if(facts.read_sh4!=Seed002::Sh4||facts.write_sh4!=Seed002::Sh4
       ||facts.read_context!=Seed002::Context||facts.write_context!=Seed002::Context
       ||facts.context_bytes!=512||facts.pointer_relative!=376||facts.pointer_bytes!=8
       ||facts.read_context+facts.pointer_relative!=Seed002::Pointer)
        return {false,"native_layout_not_proven"};
    if(get32(expected,0)!=854||get32(expected,4)!=16u*1024*1024
       ||get32(expected,Seed002::Smr)!=0||get32(expected,Seed002::Brr)!=255
       ||facts.live_smr!=0||facts.live_brr!=255
       ||get32(expected,Seed002::Scif)!=0||get32(expected,Seed002::Scif+4)!=327936
       ||get32(expected,Seed002::Scif+8)!=0xffffffffu)
        return {false,"unattributed_scif_source_or_live_registers"};
    if(!facts.handler_verified||facts.live_pointer==0
       ||get64(expected,Seed002::Pointer)!=Seed002::SavedPointer
       ||get64(loaded,Seed002::Pointer)!=facts.live_pointer)
        return {false,"sqwrite_binding_not_verified"};
    const std::uint32_t period=scif_period(facts.live_smr,facts.live_brr,facts.clock);
    if(period!=327700)return {false,"scif_baud_derivation_mismatch"};
    const std::uint32_t start=std::uint32_t(facts.cycles);
    std::uint32_t end=start+period;if(end==0xffffffffu)++end; // Native scheduler sentinel rule.
    // Transform only the saved expectation. All loaded guest timer bytes remain
    // untouched and participate in the complete postload canonical comparison.
    put32(expected,Seed002::Scif+4,start);put32(expected,Seed002::Scif+8,end);
    put64(expected,Seed002::Pointer,0);put64(loaded,Seed002::Pointer,0);
    if(expected!=loaded)return {false,"unexpected_guest_payload_difference"};
    return {true,"exact_source_transformed_guest_payload"};
}
}} // namespace silent_scope::dc_checkpoint
