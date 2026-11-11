#include "dreamcast/dc_aim_limits.h"
#include <cassert>
#include <map>
#include <fstream>
#include <vector>
using namespace silent_scope::dc_input;
int main(int argc,char** argv){
    auto native=limits_for_factor(1),wide=limits_for_factor(3);
    assert(native.yaw==5500&&native.pitch==4300&&wide.yaw==10961&&wide.pitch==9591);
    assert(limits_for_factor(0).yaw==5500&&limits_for_factor(NAN).yaw==5500);
    std::map<uint32_t,uint16_t> ram;
    const uint16_t code[]={0x902f,0x9430,0x03ed,0x3347,0x8b00,0x0e45,0x03ed,0x942b,
        0x3343,0x8900,0x0e45,0x9025,0x9427,0x03ed,0x3347,0x8b00,0x0e45,0x03ed,
        0x9422,0x3343,0x8900,0x0e45,0x4f26};
    for(unsigned i=0;i<23;++i)ram[0x8c073e7eu+i*2]=code[i];
    ram[0x8c073ee0]=0x06fe;ram[0x8c073ee2]=0x06fc;
    ram[0x8c073ee4]=5500;ram[0x8c073ee6]=uint16_t(-5500);
    ram[0x8c073ee8]=4300;ram[0x8c073eea]=uint16_t(-4300);
    int stores=0;auto read=[&](uint32_t a){return ram[a];};
    auto write=[&](uint32_t a,uint16_t v){++stores;ram[a]=v;};
    AimLimitPatch patch;assert(!patch.apply("wrong",true,read,write)&&stores==0);
    assert(patch.apply("T9507N",false,read,write)&&stores==0);
    assert(patch.apply("T9507N",true,read,write)&&stores==4&&patch.wider());
    assert(ram[0x8c073ee4]==10961&&int16_t(ram[0x8c073eea])==-9591);
    assert(patch.apply("T9507N",true,read,write)&&stores==4);
    ram[0x8c073e80]^=1;assert(!patch.apply("T9507N",false,read,write)&&stores==4);
    ram[0x8c073e80]^=1;assert(patch.apply("T9507N",false,read,write)&&stores==8);
    assert(!patch.wider()&&int16_t(ram[0x8c073ee6])==-5500);
    ram[0x8c073ee4]=123;assert(!patch.apply("T9507N",true,read,write)&&stores==8);
    if(argc>1){
        std::ifstream input(argv[1],std::ios::binary);assert(input);
        std::vector<unsigned char> boot((std::istreambuf_iterator<char>(input)),{});
        assert(boot.size()==1386016);
        auto owned_read=[&](uint32_t address){const auto offset=address-0x8c010000u;
            assert(offset+1<boot.size());return uint16_t(boot[offset]|unsigned(boot[offset+1])<<8);};
        AimLimitPatch owned;assert(owned.guard("T9507N",owned_read));
    }
}
