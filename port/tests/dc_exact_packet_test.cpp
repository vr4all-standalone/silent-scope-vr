#include "dreamcast/dc_exact_packet.h"
#include "dreamcast/dc_lens_ownership.h"
#include <cassert>
#include <vector>
using namespace silent_scope::dreamcast;
int main(){
    uint32_t packet=99;
    assert(native_vertex_payload_packet_offset(0x1000,0x1004,64,packet)&&packet==0);
    assert(native_vertex_payload_packet_offset(0x1000,0x1024,64,packet)&&packet==32);
    assert(!native_vertex_payload_packet_offset(0x1000,0x1000,64,packet));
    assert(!native_vertex_payload_packet_offset(0x1000,0x1005,64,packet));
    assert(!native_vertex_payload_packet_offset(0x1000,0x1044,64,packet));
    assert(!native_vertex_payload_packet_offset(UINTPTR_MAX-3,0,64,packet));
    SubmittedLens lens;lens.supported=lens.valid=true;lens.generation=7;lens.width=640;lens.height=480;
    lens.cx=320;lens.cy=240;lens.factor=1;lens.fx=lens.fy=555;
    ProjectionRangeLedger<64> owners;owners.reset(7);owners.append(0,32,lens);owners.append(32,32,{});
    assert(!owners.lookup(4,32).valid); // old payload lookup crossed into unowned EOL
    assert(native_vertex_payload_packet_offset(0x1000,0x1004,64,packet));
    assert(owners.lookup(packet,32).valid); // only the actual native vertex packet is queried
    std::vector<uint8_t> ram(ExactPacketRamSize);
    for(unsigned i=0;i<ram.size();++i)ram[i]=uint8_t(i*13);
    ExactPacketSnapshots<2> snapshots;snapshots.reset(7);
    assert(!snapshots.matches(0x1000,ram.data()+0x1000,32,7));
    assert(snapshots.capture(0x1000,64,ram.data(),ram.size(),7));
    assert(snapshots.matches(0x1000,ram.data()+0x1000,32,7));
    assert(snapshots.matches(0x1020,ram.data()+0x1020,32,7));
    assert(!snapshots.matches(0x1040,ram.data()+0x1040,32,7));
    assert(!snapshots.matches(0x1000,ram.data()+0x1000,32,8));
    assert(!snapshots.matches(0x1001,ram.data()+0x1001,32,7));
    assert(!snapshots.matches(0x1000,ram.data()+0x1000,31,7));
    // Any changed byte in the full native packet invalidates prior ownership.
    for(unsigned byte=0;byte<32;++byte){ram[0x1000+byte]^=0x80;
        assert(!snapshots.matches(0x1000,ram.data()+0x1000,32,7));ram[0x1000+byte]^=0x80;}
    assert(snapshots.captured_packet(0x1000,7)&&!snapshots.captured_packet(0x1000,8));
    ram[0x1000]^=1;assert(snapshots.captured_packet(0x1000,7)[0]!=ram[0x1000]);assert(snapshots.capture(0x1000,32,ram.data(),ram.size(),7));
    assert(snapshots.matches(0x1000,ram.data()+0x1000,32,7)); // new completed output replaces old bytes
    assert(snapshots.capture(0x1fe0,64,ram.data(),ram.size(),7)); // page crossing
    assert(snapshots.pages()==2&&snapshots.matches(0x2000,ram.data()+0x2000,32,7));
    assert(!snapshots.capture(0x3000,32,ram.data(),ram.size(),7));
    assert(snapshots.overflowed()&&!snapshots.matches(0x1000,ram.data()+0x1000,32,7));
    snapshots.reset(8);assert(!snapshots.overflowed()&&snapshots.pages()==0);
    assert(!snapshots.matches(0x1000,ram.data()+0x1000,32,8));
    assert(!snapshots.capture(0x1001,32,ram.data(),ram.size(),8));
    assert(!snapshots.capture(0x1000,31,ram.data(),ram.size(),8));
    assert(!snapshots.capture(ExactPacketRamSize,32,ram.data(),ram.size(),8));
    assert(!snapshots.capture(UINT32_MAX-31,64,ram.data(),ram.size(),8));
    assert(!snapshots.capture(0,32,nullptr,ram.size(),8));
    assert(!snapshots.capture(0,32,ram.data(),31,8));
    assert(!snapshots.capture(0,32,ram.data(),ram.size(),7));
    assert(snapshots.capture(ExactPacketRamSize-32,32,ram.data(),ram.size(),8));
    assert(snapshots.matches(ExactPacketRamSize-32,ram.data()+ExactPacketRamSize-32,32,8));
}

