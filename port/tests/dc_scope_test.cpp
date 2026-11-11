#include "dreamcast/dc_scope.h"
#include <cassert>
#include <iostream>
#include <thread>
using namespace silent_scope::dc_scope;
namespace {
unsigned copies{}, destroys{},creates{};
Target create() { ++creates;return {12,34}; }
CopyResult copy(Target target,unsigned source_fbo) { assert(target.texture==12 && target.framebuffer==34 && source_fbo==55); ++copies; return {true,0}; }
void destroy(Target) { ++destroys; }
Source source() { return {"T9507N",true,true,640,480,0x52c000,1,512,0,0,256,256,10,3,55}; }
}
int main() {
    ScopeGpu owner;
    auto s=source();
    assert(is_native_scope(s));
    auto wrong=s; wrong.destination+=32; assert(!is_native_scope(wrong));
    wrong=s; wrong.product="T9505D"; assert(!is_native_scope(wrong));
    wrong=s; wrong.clip_width=255; assert(!is_native_scope(wrong));
    wrong=s; wrong.stride=1280; assert(!is_native_scope(wrong));
    wrong=s; wrong.native_copy=false; assert(!is_native_scope(wrong));
    assert(owner.initialize({create,copy,destroy},77));
    assert(!owner.capture(s,77,10)); // physical rifle is opt-in
    owner.set_physical_rifle_active(true);
    assert(owner.capture(s,77,10));
    auto view=owner.acquire(11,3,77,10.03);
    assert(view && view.texture==12 && view.width==256 && view.height==256 && view.origin_bottom_left);
    assert(view.submit_sequence==10 && view.native_generation==3 && copies==1);
    assert(view.content_top_at_v0);
    // Native drains may finish RTT(next) before main(next). Keep main(current)'s
    // compatible image rather than blacking it out or borrowing future pixels.
    s.submit_sequence=12;assert(owner.capture(s,77,10.035));
    auto retained=owner.acquire(11,3,77,10.04);
    assert(retained && retained.submit_sequence==10);
    assert(owner.acquire(13,3,77,10.04).submit_sequence==12);
    auto unrelated=s;unrelated.destination+=32;assert(!owner.capture(unrelated,77,10.04));
    assert(owner.acquire(13,3,77,10.04).submit_sequence==12);
    assert(!owner.acquire(9,3,77,10.03)); // never attach a later scope to an older world
    assert(!owner.acquire(17,3,77,10.04)); // bounded submission gap
    assert(!owner.acquire(11,4,77,10.03));
    assert(!owner.acquire(11,3,78,10.03));
    assert(!owner.acquire(11,3,77,10.3));
    assert(!owner.acquire(11,3,77,9.9));
    std::thread wrong_thread([&] { assert(!owner.acquire(11,3,77,10.03)); assert(!owner.capture(s,77,10.04)); });
    wrong_thread.join();
    assert(copies==2);
    std::thread native_reset([&] { owner.invalidate(); }); native_reset.join();
    assert(!owner.acquire(11,3,77,10.03));
    assert(!owner.acquire(13,3,77,10.04)); // both history slots invalidated
    s.submit_sequence=12; assert(owner.capture(s,77,10.04));
    owner.set_physical_rifle_active(false); assert(!owner.acquire(13,3,77,10.04));
    owner.set_physical_rifle_active(true); assert(!owner.acquire(13,3,77,10.04));
    s.submit_sequence=14; assert(owner.capture(s,77,10.05));
    assert(owner.release(77) && destroys==creates);
    const auto first_release_destroys=destroys;
    assert(!owner.acquire(15,3,77,10.05));
    assert(owner.initialize({create,copy,destroy},88));
    owner.set_physical_rifle_active(true); s.submit_sequence=16; assert(owner.capture(s,88,11));
    owner.context_lost(); assert(destroys==first_release_destroys); // never delete old names in a new/lost context
    assert(!owner.acquire(17,3,88,11));
    // Actual Quest trace: retained main1053, latest RTT1056. A second
    // queued RTT can complete before the corresponding main is presented.
    // Two slots evict1052 and make the physical optic/native-widget alternate.
    ScopeGpu queued;
    assert(queued.initialize({create,copy,destroy},99));queued.set_physical_rifle_active(true);
    s.submit_sequence=1052;assert(queued.capture(s,99,20));
    s.submit_sequence=1054;assert(queued.capture(s,99,20.01));
    s.submit_sequence=1056;assert(queued.capture(s,99,20.02));
    const auto displayed=queued.acquire(1053,3,99,20.03);
    assert(displayed&&displayed.submit_sequence==1052);
    assert(queued.acquire(1055,3,99,20.03).submit_sequence==1054);
    assert(queued.acquire(1057,3,99,20.03).submit_sequence==1056);
    assert(!queued.acquire(1051,3,99,20.03)); // future pixels still rejected
    assert(!queued.acquire(1061,3,99,20.03)); // same freshness/sequence bound
    queued.invalidate();assert(!queued.acquire(1053,3,99,20.03));
    assert(queued.release(99));
    std::cout<<"Dreamcast scope ownership/source/freshness tests passed\n";
}
