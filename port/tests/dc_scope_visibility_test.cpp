#include "dreamcast/dc_scope_visibility.h"
#include <iostream>
#include <map>
#include <stdexcept>
using namespace silent_scope::dreamcast;
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
// The original SH4 sphere path uses absolute X/Y, signed Z and >>14.
static int dot(int z,int x,int sine,int cosine){return int(std::floor(double(z*sine+std::abs(x)*cosine)/16384.));}
int main(){try{
    ScopeVisibilityFacts f;
    f.code_guard=true;f.mode=1;f.frustum=0x8c1f7554;f.live_return=0x8c058b66;
    f.outer_return=0x8c074898;f.original_span=200;f.scope_source=ScopeWorldFocalBits;
    f.planes={155,16383,157,16383};
    check(dot(-10000,150,155,16383)>0,"Fixture must be outside the original native cone");
    for(float scale:{.2f,.5f}){
        const float focal=13528.986328125f*scale;std::memcpy(&f.sdk_focal,&focal,4);
        const auto changed=decide_scope_visibility(f);
        check(changed.applied,"Matching zoom-out camera did not widen visibility");
        check(dot(-10000,150,changed.planes[0],changed.planes[1])<=0,"Newly visible enemy still rejected outside old cone");
        check(dot(-10000,-150,changed.planes[0],changed.planes[1])<=0,"Left side remains narrow");
        check(dot(-10000,150,changed.planes[2],changed.planes[3])<=0,"Vertical visibility remains narrow");
        check(dot(-10000,600,changed.planes[0],changed.planes[1])>0,"Far outside enemy bypassed frustum culling");
        for(int axis:{0,2}){
            const int s=changed.planes[axis],c=changed.planes[axis+1];
            check(s*s+c*c<=16384*16384,"Rounded normal shrinks sphere-radius coverage");
            check(double(s)/c>=double(f.planes[axis])/f.planes[axis+1]/scale,"Rounded cone is too narrow");
        }
    }
    for(float scale:{1.f,2.f,5.f}){
        const float focal=13528.986328125f*scale;std::memcpy(&f.sdk_focal,&focal,4);
        check(!decide_scope_visibility(f).applied,"Native or zoom-in visibility changed");
    }
    float focal=13528.986328125f*.2f;std::memcpy(&f.sdk_focal,&focal,4);
    const auto good=f;
    f.mode=0;check(!decide_scope_visibility(f).applied,"Main camera visibility was widened");f=good;
    f.code_guard=false;check(!decide_scope_visibility(f).applied,"Unknown code accepted");f=good;
    f.scope_source^=1;check(!decide_scope_visibility(f).applied,"Unknown scope focal accepted");f=good;
    f.original_span=201;check(!decide_scope_visibility(f).applied,"Unknown native angular span accepted");f=good;
    f.outer_return=0;check(!decide_scope_visibility(f).applied,"Unknown caller accepted");f=good;
    f.frustum=0;check(!decide_scope_visibility(f).applied,"Unknown frustum accepted");f=good;
    f.live_return=0;check(!decide_scope_visibility(f).applied,"Unknown return point accepted");f=good;
    f.planes[0]=-155;check(!decide_scope_visibility(f).applied,"Malformed plane accepted");f=good;
    f.sdk_focal=0;check(!decide_scope_visibility(f).applied,"Stale/non-camera focal accepted");f=good;
    f.outer_return=0x8c0748e8;check(decide_scope_visibility(f).applied,"Second verified actor-list caller rejected");
    std::map<uint32_t,uint32_t> words;for(auto g:ScopeVisibilitySite::WordGuard)words[g.first]=g.second;
    auto rd32=[&](uint32_t a){return words.at(a);};
    auto rd16=[&](uint32_t a){return a==ScopeVisibilitySite::TargetPc?ScopeVisibilitySite::TrapOpcode:a==0x8c058b64?uint16_t(9):uint16_t(0x1ec5);};
    check(ScopeVisibilitySite::guarded(true,rd32,rd16),"Verified hook guard rejected");
    words[ScopeVisibilitySite::WordGuard[0].first]^=1;
    check(!ScopeVisibilitySite::guarded(true,rd32,rd16),"Changed native code accepted");
    std::cout<<"Native scope visibility: old-cone rejection, wider enemies, radius coverage, retained culling and strict guards pass\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
