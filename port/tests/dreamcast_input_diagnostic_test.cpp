#include <iostream>
#include <cstring>
#include <vector>
#include <limits>
#if __has_include("dreamcast/dc_input_diagnostic.h")
#include "dreamcast/dc_input_diagnostic.h"
#define HAS_DC_INPUT_DIAGNOSTIC 1
#endif
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
#ifndef HAS_DC_INPUT_DIAGNOSTIC
    REQUIRE(false && "Native mouse diagnostic missing");
#else
    using namespace silent_scope::dc_input_diagnostic;
    REQUIRE(RotationPairOffset==0x1b0860);
    REQUIRE(parse_step(nullptr)==32&&parse_step("16")==16&&parse_step("48")==48&&parse_step("32")==32);
    REQUIRE(parse_step("-16")==0&&parse_step("16 ")==0&&parse_step("0")==0&&parse_step("64")==0);
    for(int counts:{16,48}) {
        Script scaled(true,counts);scaled.poll("T9507N",100);
        REQUIRE(scaled.poll("T9507N",120).pressed==Middle);
        REQUIRE(scaled.poll("T9507N",121).pressed==Left);
        for(unsigned phase=0;phase<8;++phase){
            const double at=142+2*phase;auto sample=scaled.poll("T9507N",at);
            REQUIRE(sample.x==(phase<4?(phase%2?-counts:counts):0));
            REQUIRE(sample.y==(phase>=4?(phase%2?-counts:counts):0));
            auto next=scaled.poll("T9507N",at+.01);REQUIRE(next.x==0&&next.y==0);
        }
        REQUIRE(scaled.poll("T9507N",158).pressed==Right);
    }
    Script disabled(false);REQUIRE(!disabled.poll("T9507N",0).owned);
    Script script(true);REQUIRE(!script.poll("OTHER",50).owned);
    REQUIRE(!script.poll("T9507N",std::numeric_limits<double>::quiet_NaN()).owned);
    auto value=script.poll("T9507N",100);REQUIRE(value.owned&&value.pressed==Left);
    REQUIRE(script.poll("T9507N",100.11).pressed==0);
    REQUIRE(script.poll("T9507N",120).pressed==Middle);
    REQUIRE(script.poll("T9507N",120.11).pressed==0);
    REQUIRE(script.poll("T9507N",121).pressed==Left);
    REQUIRE(script.poll("T9507N",128).pressed==0);
    value=script.poll("T9507N",142);REQUIRE(value.x==32&&value.y==0);
    REQUIRE(script.poll("T9507N",142.01).x==0); // Relative movement is an impulse, not held per-poll speed.
    REQUIRE(script.poll("T9507N",144).x==-32);
    REQUIRE(script.poll("T9507N",146).x==32);
    REQUIRE(script.poll("T9507N",148).x==-32);
    REQUIRE(script.poll("T9507N",150).y==32);
    REQUIRE(script.poll("T9507N",152).y==-32);
    REQUIRE(script.poll("T9507N",154).y==32);
    REQUIRE(script.poll("T9507N",156).y==-32);
    REQUIRE(script.poll("T9507N",158).pressed==Right);
    REQUIRE(script.poll("T9507N",160).pressed==Middle);
    REQUIRE(script.poll("T9507N",162).pressed==Left);
    value=script.poll("T9507N",164);REQUIRE(!value.x&&!value.y&&!value.pressed);
    REQUIRE(!script.poll("OTHER",165).owned);
    REQUIRE(!script.poll("T9507N",166).owned); // Content changes cannot restart the experiment.
    Script late(true);late.poll("T9507N",0);value=late.poll("T9507N",43);
    REQUIRE(value.x==0);REQUIRE(late.poll("T9507N",42).x==0);
    std::vector<std::uint32_t> ram(32,0);ram[1]=100;
    float f=50;std::memcpy(&ram[2],&f,4);
    AxisScan scan;REQUIRE(!scan.baseline(nullptr,128));
    REQUIRE(!scan.baseline(ram.data(),MaxRamBytes+4));
    REQUIRE(scan.baseline(ram.data(),ram.size()*4));
    REQUIRE(scan.observe(ram.data(),ram.size()*4,0)); // No movement => no candidates.
    REQUIRE(scan.candidate_count()==0);
    REQUIRE(scan.baseline(ram.data(),ram.size()*4));
    for(unsigned phase=0;phase<4;++phase){
        ram[1]=phase%2?100:110;f=phase%2?50:52;std::memcpy(&ram[2],&f,4);
        ram[3]=phase+1; // Unrelated monotonic counter must not correlate.
        REQUIRE(scan.observe(ram.data(),ram.size()*4,phase));
    }
    auto results=scan.results();bool found_int=false,found_float=false;
    for(const auto& result:results){
        found_int|=result.offset==4&&result.kind==Kind::Signed;
        found_float|=result.offset==8&&result.kind==Kind::Float;
        REQUIRE(result.offset!=12);
    }
    REQUIRE(found_int&&found_float);
    REQUIRE(!scan.observe(ram.data(),ram.size()*4,3)); // Duplicate/out-of-order sample cannot fabricate correlation.
    REQUIRE(!reverses({0,1,2,3,4},Kind::Signed));
    REQUIRE(!reverses({0,1,0,1,std::numeric_limits<double>::quiet_NaN()},Kind::Float));
    REQUIRE(reverses({100,110,100,110,100},Kind::Signed));
    REQUIRE(!plausible(65537,Kind::Signed)&&!plausible(-65537,Kind::Float));
    REQUIRE(!reverses({100,110,102,112,104},Kind::Signed)); // Nearby returns are insufficient.
    REQUIRE(!reverses({100,110,100,120,100},Kind::Signed)); // Identical input must repeat its gain.
    std::vector<std::uint32_t> crowded(MaxCandidates+1,0);AxisScan capped;
    REQUIRE(capped.baseline(crowded.data(),crowded.size()*4));
    for(auto& word:crowded)word=100;
    REQUIRE(capped.observe(crowded.data(),crowded.size()*4,0));
    REQUIRE(capped.candidate_count()==MaxCandidates&&capped.dropped()==1);
    REQUIRE(capped.results().empty()); // An incomplete experiment never yields verified-looking candidates.
    for(unsigned phase=1;phase<4;++phase){
        for(auto& word:crowded)word=phase%2?0:100;
        REQUIRE(capped.observe(crowded.data(),crowded.size()*4,phase));
    }
    REQUIRE(capped.results().empty()); // Overflow must suppress even perfect-looking prefix matches.
    capped.release();REQUIRE(capped.candidate_count()==0);
    std::vector<std::uint32_t> noisy(32768,100000000);noisy.back()=100;
    AxisScan high;REQUIRE(high.baseline(noisy.data(),noisy.size()*4));
    for(unsigned phase=0;phase<4;++phase){
        for(auto& word:noisy)word=100001000+phase*1000;
        noisy.back()=phase%2?100:110;
        REQUIRE(high.observe(noisy.data(),noisy.size()*4,phase));
    }
    REQUIRE(high.dropped()==0&&!high.results().empty());
    for(const auto& result:high.results())REQUIRE(result.offset==(noisy.size()-1)*4);
    std::vector<std::uint32_t> packed(1,(200u<<16)|100);AxisScan halves;
    REQUIRE(halves.baseline(packed.data(),4));
    for(unsigned phase=0;phase<4;++phase){packed[0]=(200u<<16)|(phase%2?100:110);REQUIRE(halves.observe(packed.data(),4,phase));}
    bool found_half=false;
    for(const auto& result:halves.results())found_half|=result.kind==Kind::Signed16Low&&result.offset==0;
    REQUIRE(found_half); // Actual title loads paired signed16 rotation fields, not one signed32 value.
    std::vector<std::uint32_t> ranked(129,0);float initial=100;
    for(auto& word:ranked)std::memcpy(&word,&initial,4);
    AxisScan best;REQUIRE(best.baseline(ranked.data(),ranked.size()*4));
    for(unsigned phase=0;phase<4;++phase){
        float ordinary=phase==0?110:phase==1?100.05f:phase==2?110.05f:100.10f;
        for(auto& word:ranked)std::memcpy(&word,&ordinary,4);
        float exact=phase%2?100:110;std::memcpy(&ranked.back(),&exact,4);
        REQUIRE(best.observe(ranked.data(),ranked.size()*4,phase));
    }
    std::size_t matches=0;const auto top=best.results(&matches);
    REQUIRE(matches>=129&&top.size()==MaxResults);
    REQUIRE(top.front().offset==128*4&&top.front().error==0); // Best high-address match survives the output limit.
#endif
    return 0;
}
