#include "dreamcast/dc_wider_control.h"
#include <cassert>
using namespace silent_scope::dreamcast;
int main(){
    LensRequest request{true,true,true,8};
    auto eval=[&](uint32_t loaded=0x440ac000,uint32_t caller=0x8c019422,uint32_t mode=0,uint32_t fpscr=0,bool guard=true,bool fd=false){
        return decide_main_wider(request,loaded,caller,mode,fpscr,guard,fd,true);};
    assert(production_camera_enabled(request)==ProductionCameraCertified);
    assert(!production_camera_enabled(request,false));
    assert(decide_main_wider(request,0x440ac000,0x8c019422,0,0,true,false).store.eligible==ProductionCameraCertified);
    assert(!decide_main_wider(request,0x440ac000,0x8c019422,0,0,true,false,false).store.eligible);
    auto wide=eval();assert(wide.store.eligible&&wide.store.applied&&wide.store.effective_bits==0x43390000&&wide.factor==3&&wide.revision==8);
    assert(!eval(0x43390000).store.eligible); // raw stock only; never compounds
    assert(!eval(0x440ac000,0x8c019422,1).store.eligible); // scope source untouched
    assert(!eval(0x440ac000,0x8c01900e).store.eligible);
    assert(!eval(0x440ac000,0x8c019422,0,1u<<19).store.eligible);
    assert(!eval(0x440ac000,0x8c019422,0,1u<<20).store.eligible);
    assert(!eval(0x440ac000,0x8c019422,0,0,true,true).store.eligible); // FD native fallback
    assert(!eval(0x440ac000,0x8c019422,0,0,false).store.eligible);
    request.wider=false;++request.revision;auto off=eval();assert(off.store.eligible&&!off.store.applied&&off.store.effective_bits==0x440ac000&&off.factor==1&&off.revision==9);
    request.wider=true;request.wide_proven=false;assert(eval().factor==1); // stock VR can be supported beforewide proof
    request.enabled=false;assert(!eval().store.eligible);
    NativeWiderApplicationTracker tracker;
    assert(tracker.changed(true,3,8));assert(!tracker.changed(true,3,8));
    // Re-enable at the same actual factor must re-acknowledge the new request.
    assert(tracker.changed(true,3,10));assert(!tracker.changed(true,3,10));
    assert(tracker.changed(true,1,11));assert(tracker.changed(false,1,11));
    assert(!tracker.changed(false,1,11));assert(tracker.changed(false,1,12));
    tracker.reset();assert(tracker.changed(true,1,12));
}
