#include "dreamcast/dc_camera_focal_probe.h"
#include "dreamcast/dc_scope_camera.h"
#include <cassert>
#include <map>
using namespace silent_scope::dreamcast;
struct Registers {std::array<uint32_t,16> fr{};uint32_t& fr_hex(int n){return fr[n];}};
int main(){
    for(float scale:{.2f,.5f,1.f,2.f,5.f}){
        const auto scope=decide_scope_camera(scale,ScopeWorldFocalBits,FocalSetterProbe::CallerReturn,1,0,true,false);
        assert(scope.eligible&&scope.applied==(scale!=1.f));
        float actual;
        std::memcpy(&actual,&scope.effective_bits,sizeof actual);
        assert(actual==13528.986328125f*scale);
    }
    assert(!decide_scope_camera(5,0x43946231,FocalSetterProbe::CallerReturn,1,0,true,false).eligible);
    const auto indoor=decide_scope_camera(2.f,0x461a8a85,FocalSetterProbe::CallerReturn,1,0,true,false);
    assert(indoor.applied&&indoor.effective_bits==0x469a8a85);
    assert(!decide_scope_camera(.2f,ScopeWorldFocalBits,FocalSetterProbe::CallerReturn,0,0,true,false).eligible);
    assert(!decide_scope_camera(.2f,ScopeWorldFocalBits,FocalSetterProbe::CallerReturn,1,0,false,false).eligible);
    assert(!decide_scope_camera(.2f,ScopeWorldFocalBits,0x8c019092,1,0,true,false).eligible);
    assert(!FocalSetterProbe(3).evaluate(46,0x440ac000,FocalSetterProbe::CallerReturn,0,0,true,true).eligible);
    FocalSetterProbe trial(3),control(1);
    auto result=trial.evaluate(50,0x440ac000,0x8c019422,0,0,true);
    assert(result.eligible&&result.applied&&result.effective_bits==0x43390000);
    assert(!trial.evaluate(55,0x440ac000,0x8c019422,0,0,true).applied);
    assert(!trial.evaluate(44.99,0x440ac000,0x8c019422,0,0,true).applied);
    assert(control.evaluate(50,0x440ac000,0x8c019422,0,0,true).eligible);
    assert(!control.evaluate(50,0x440ac000,0x8c019422,0,0,true).applied);
    assert(!trial.evaluate(50,0x440ac000,0x8c019422,1,0,true).eligible);
    assert(!trial.evaluate(50,0x43946231,0x8c019422,1,0,true).eligible);
    assert(!trial.evaluate(50,0x440ac000,0x8c01900e,0,0,true).eligible);
    assert(!trial.evaluate(50,0x440ac000,0x8c019422,0,1u<<20,true).eligible);
    assert(!trial.evaluate(50,0x440ac000,0x8c019422,0,1u<<19,true).eligible);
    assert(trial.evaluate(50,0x440ac000,0x8c019422,0,1u<<21,true).applied);
    assert(!trial.evaluate(50,0x43390000,0x8c019422,0,0,true).eligible);
    assert(!trial.evaluate(50,0x440ac000,0x8c019422,0,0,false).eligible);
    std::map<uint32_t,uint32_t> words;for(auto g:CameraSampler::CodeGuard)words[g.first]=g.second;
    for(auto g:FocalSetterProbe::WordGuard)words[g.first]=g.second;
    std::map<uint32_t,uint16_t> halves;for(auto g:FocalSetterProbe::HalfGuard)halves[g.first]=g.second;
    halves[FocalSetterProbe::TargetPc]=FocalSetterProbe::OriginalOpcode;
    auto rd32=[&](uint32_t a){return words.at(a);};auto rd16=[&](uint32_t a){return halves.at(a);};
    assert(FocalSetterProbe::guarded("T9507N",false,rd32,rd16));
    halves[FocalSetterProbe::TargetPc]=FocalSetterProbe::TrapOpcode;
    assert(FocalSetterProbe::guarded("T9507N",true,rd32,rd16));
    halves[FocalSetterProbe::HalfGuard[0].first]^=1;assert(!FocalSetterProbe::guarded("T9507N",true,rd32,rd16));
    halves[FocalSetterProbe::HalfGuard[0].first]^=1;words[FocalSetterProbe::WordGuard[0].first]^=1;assert(!FocalSetterProbe::guarded("T9507N",true,rd32,rd16));
    Registers registers;for(unsigned i=0;i<16;++i)registers.fr[i]=0x10203000+i;const auto original=registers.fr;
    try{CameraFocalRegisterRestore restore(&registers,0x43390000);assert(registers.fr[4]==0x43390000);throw 1;}catch(int){}
    assert(registers.fr==original); // temporary effective FR4 restores on exceptions
    {CameraFocalRegisterRestore restore(&registers,0x43390000);assert(registers.fr[4]==0x43390000);}
    assert(registers.fr==original);
}
