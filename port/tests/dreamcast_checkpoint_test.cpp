#include <iostream>
#include <limits>
#if __has_include("dreamcast/dc_checkpoint_policy.h")
#include "dreamcast/dc_checkpoint_policy.h"
#define HAS_DC_CHECKPOINT 1
#endif
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
#ifndef HAS_DC_CHECKPOINT
    REQUIRE(false && "Checkpoint policy missing");
#else
    using namespace silent_scope::dc_checkpoint;
    REQUIRE(parse_mode(nullptr)==Mode::None&&parse_mode("save40")==Mode::Save);
    REQUIRE(parse_mode("load30")==Mode::Load&&parse_mode("SAVE40")==Mode::None);
    Policy disabled;disabled.observe("T9507N",100);REQUIRE(disabled.take()==Mode::None);
    Policy save(Mode::Save);save.observe("OTHER",40);REQUIRE(save.take()==Mode::None);
    save.observe("T9507N",std::numeric_limits<double>::quiet_NaN());REQUIRE(save.take()==Mode::None);
    save.observe("T9507N",39.9);REQUIRE(save.take()==Mode::None);
    save.observe("T9507N",40.01);REQUIRE(save.take()==Mode::Save);
    save.observe("T9507N",41);REQUIRE(save.take()==Mode::None);
    Policy load(Mode::Load);load.observe("T9507N",30);REQUIRE(load.take()==Mode::Load);
    load.observe("T9507N",40);REQUIRE(load.take()==Mode::None); // State-load events never re-arm.
    Header h;h.version=854;h.bytes=1024;h.cycles=8000000000ull;h.ram_bytes=16*1024*1024;
    h.payload_md5[3]=42;h.ram_md5[5]=71;const auto encoded=encode(h);Header decoded;
    REQUIRE(decode(encoded,decoded));REQUIRE(decoded.version==854&&decoded.bytes==1024&&decoded.cycles==h.cycles);
    REQUIRE(decoded.payload_md5==h.payload_md5&&decoded.ram_md5==h.ram_md5);
    auto bad=encoded;bad[0]^=1;REQUIRE(!decode(bad,decoded));
    bad=encoded;bad[36]='X';REQUIRE(!decode(bad,decoded));
    h.bytes=MaxStateBytes+1;REQUIRE(!decode(encode(h),decoded));
    h.bytes=0;REQUIRE(!decode(encode(h),decoded));
    std::vector<std::uint8_t> saved(100,0),loaded(100,0);loaded[3]=7;loaded[4]=8;loaded[90]=9;
    auto diff=compare_payloads(saved,loaded);
    REQUIRE(diff.different==3&&diff.range_count==2&&diff.bytes.size()==3&&diff.ranges.size()==2);
    REQUIRE(diff.ranges[0].offset==3&&diff.ranges[0].length==2&&diff.ranges[1].offset==90);
    for(auto& b:loaded)b=1;
    diff=compare_payloads(saved,loaded);
    REQUIRE(diff.different==100&&diff.bytes.size()==64&&diff.range_count==1);
    loaded.resize(101);diff=compare_payloads(saved,loaded);REQUIRE(diff.different==101);
    REQUIRE(compare_payloads(saved,saved).different==0);
#endif
    return 0;
}
