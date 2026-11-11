#include "dreamcast/dc_checkpoint_policy.h"
#include <iostream>
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
using namespace silent_scope::dc_checkpoint;
int main(){
    std::vector<std::uint8_t> saved(Seed002::PayloadBytes,0);
    put32(saved,0,854);put32(saved,4,16u*1024*1024);
    put32(saved,Seed002::Smr,0);put32(saved,Seed002::Brr,255);
    put32(saved,Seed002::Scif,0);put32(saved,Seed002::Scif+4,327936);put32(saved,Seed002::Scif+8,0xffffffff);
    put64(saved,Seed002::Pointer,Seed002::SavedPointer);
    PostloadFacts facts;facts.cycles=Seed002::Cycles;facts.version=854;facts.clock=200000000;
    facts.read_sh4=facts.write_sh4=Seed002::Sh4;
    facts.read_context=facts.write_context=Seed002::Context;
    facts.context_bytes=512;facts.pointer_relative=376;facts.pointer_bytes=8;
    facts.live_smr=0;facts.live_brr=255;facts.live_pointer=0x7ff600001000;facts.handler_verified=true;
    auto loaded=saved;put32(loaded,Seed002::Scif+4,std::uint32_t(facts.cycles));
    put32(loaded,Seed002::Scif+8,std::uint32_t(facts.cycles)+327700);put64(loaded,Seed002::Pointer,facts.live_pointer);
    auto expected=saved,canonical=loaded;auto result=canonical_postload(expected,canonical,facts);
    REQUIRE(result.equal&&expected==canonical);
    REQUIRE(get32(canonical,Seed002::Scif+4)==std::uint32_t(facts.cycles)); // Guest scheduler bytes are retained.
    REQUIRE(get64(canonical,Seed002::Pointer)==0);
    canonical=loaded;canonical[1000000]^=1;expected=saved;REQUIRE(!canonical_postload(expected,canonical,facts).equal);
    canonical=loaded;canonical[Seed002::Scif+8]^=1;expected=saved;REQUIRE(!canonical_postload(expected,canonical,facts).equal);
    canonical=loaded;expected=saved;put32(expected,Seed002::Scif+4,1);REQUIRE(!canonical_postload(expected,canonical,facts).equal);
    canonical=loaded;expected=saved;auto unknown=facts;unknown.live_brr=254;REQUIRE(!canonical_postload(expected,canonical,unknown).equal);
    unknown=facts;unknown.pointer_relative=384;REQUIRE(!canonical_postload(expected,canonical,unknown).equal);
    unknown=facts;unknown.read_context++;REQUIRE(!canonical_postload(expected,canonical,unknown).equal);
    unknown=facts;unknown.handler_verified=false;REQUIRE(!canonical_postload(expected,canonical,unknown).equal);
    unknown=facts;unknown.live_pointer++;REQUIRE(!canonical_postload(expected,canonical,unknown).equal);
    REQUIRE(scif_period(0,255,200000000)==327700);
    return 0;
}
