// Run through private/flycast-research/run_checkpoint_serializer_test.py, which
// compiles the real headers and verbatim native constructor definitions. The
// unit does not link the emulator or pretend to run a game.
#include "serialize.h"
#include "dreamcast/dc_checkpoint_policy.h"
#include <iostream>
#include <vector>
settings_t settings;
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
    settings.platform.system=DC_PLATFORM_DREAMCAST;
    settings.platform.ram_size=16u*1024*1024;
    Serializer count;
    REQUIRE(count.dryrun()&&!count.rollback()&&count.size()==8);
    // ta_ctx.cpp::serializeContext deliberately reserves its maximum in dryrun.
    count.skip(4+4+8u*1024*1024);
    std::vector<std::uint8_t> payload(count.size());
    Serializer writer(payload.data(),payload.size());
    REQUIRE(!writer.dryrun()&&!writer.rollback()&&writer.size()==8);
    std::uint32_t address=0x00200000,used=32;std::array<std::uint8_t,32> fixture{};
    writer<<address;writer<<used;writer.serialize(fixture.data(),fixture.size());
    REQUIRE(writer.size()==48&&writer.size()<count.size());
    REQUIRE(silent_scope::dc_checkpoint::trim_payload(payload,writer.size()));
    REQUIRE(payload.size()==48);
    Deserializer reader(payload.data(),payload.size());
    std::uint32_t actual_address=0,actual_used=0;reader>>actual_address;reader>>actual_used;
    std::array<std::uint8_t,32> actual{};reader.deserialize(actual.data(),actual.size());
    REQUIRE(reader.size()==payload.size()&&actual_address==address&&actual_used==used&&actual==fixture);
    REQUIRE(!silent_scope::dc_checkpoint::trim_payload(payload,payload.size()+1));
    REQUIRE(!silent_scope::dc_checkpoint::trim_payload(payload,7));
    std::cout<<"real_serializer_capacity="<<count.size()<<" written="<<writer.size()<<" complete_deserialize="<<reader.size()<<"\n";
    return 0;
}
