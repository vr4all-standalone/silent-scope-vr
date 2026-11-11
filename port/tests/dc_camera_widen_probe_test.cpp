#include "dreamcast/dc_camera_widen_probe.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <map>
using namespace silent_scope::dreamcast;
int main(){
    CameraWidenProbe policy(3);
    auto decide=[&](double time,uint32_t bits=0x440ac000,uint32_t r3=0x8c1b0108,uint32_t fpscr=0,uint32_t mode=0,bool guard=true){
        return policy.evaluate(time,bits,r3,fpscr,mode,guard);
    };
    assert(decide(45).applied && decide(45).effective_bits==0x43390000);
    assert(decide(54.999).applied);
    assert(!decide(44.999).applied && decide(44.999).effective_bits==0x440ac000);
    assert(!decide(55).applied && decide(55).effective_bits==0x440ac000);
    assert(!decide(std::numeric_limits<double>::quiet_NaN()).applied);
    assert(!decide(50,0x43390000).applied); // never compound our185
    assert(!decide(50,0x440a0000).applied); // unobserved raw focal
    assert(!decide(50,0x440ac000,0x8c1b0110).applied); // scope source
    assert(!decide(50,0x440ac000,0x8c1b0108,1u<<20).applied); // SZ64bit move
    assert(!decide(50,0x440ac000,0x8c1b0108,1u<<19).applied); // PR unsupported
    assert(decide(50,0x440ac000,0x8c1b0108,1u<<21).applied); // current FR bank, original load handles it
    assert(!decide(50,0x440ac000,0x8c1b0108,0,1).applied);
    assert(!decide(50,0x440ac000,0x8c1b0108,0,2).applied);
    assert(!decide(50,0x440ac000,0x8c1b0108,0,0,false).applied);
    for(int factor:{-1,0,1,2,4,10}) assert(!CameraWidenProbe(factor).evaluate(50,0x440ac000,0x8c1b0108,0,0,true).applied);
    assert(CameraWidenProbe(1).enabled());
    assert(!CameraWidenProbe(2).enabled());
    std::map<uint32_t,uint32_t> words;
    for(auto entry:CameraSampler::CodeGuard)words[entry.first]=entry.second;
    words[0x8c01915c]=0x8c1b0108;
    words[0x8c019160]=0x8c112864;
    words[0x8c019144]=0x8c1b0114;
    std::map<uint32_t,uint16_t> halves;
    for(auto entry:CameraWidenProbe::SetupGuard)halves[entry.first]=entry.second;
    halves[CameraWidenProbe::TargetPc]=CameraWidenProbe::OriginalOpcode;
    auto rd32=[&](uint32_t address){return words.at(address);};
    auto rd16=[&](uint32_t address){return halves.at(address);};
    assert(CameraWidenProbe::guarded("T9507N",false,rd32,rd16));
    assert(!CameraWidenProbe::guarded("UNKNOWN",false,rd32,rd16));
    halves[CameraWidenProbe::TargetPc]=CameraWidenProbe::TrapOpcode;
    assert(!CameraWidenProbe::guarded("T9507N",false,rd32,rd16));
    assert(CameraWidenProbe::guarded("T9507N",true,rd32,rd16));
    halves[CameraWidenProbe::SetupGuard[2].first]^=1;
    assert(!CameraWidenProbe::guarded("T9507N",true,rd32,rd16));
    halves[CameraWidenProbe::SetupGuard[2].first]^=1;
    words[CameraSampler::CodeGuard[0].first]^=1;
    assert(!CameraWidenProbe::guarded("T9507N",true,rd32,rd16));
}
