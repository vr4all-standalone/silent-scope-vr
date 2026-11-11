#include "display_bridge.h"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace silent_scope;
#define REQUIRE(x) do { if(!(x)){std::cerr << "FAIL line " << __LINE__ << ": " #x "\n";return 1;} } while(false)
RasterFrame frame(uint64_t session, uint64_t sequence, double time, uint32_t color) {
    return {session,sequence,time,2,2,4.0/3.0,{color,color,color,color}};
}
int main() {
    DisplayBridge bridge;
    auto generation=bridge.generation();
    REQUIRE(!bridge.acquire(Display::Main));
    REQUIRE(bridge.publish(Display::Main,frame(generation,1,.1,0xff0000)));
    auto main=bridge.acquire(Display::Main);
    REQUIRE(bridge.publish(Display::Scope,frame(generation,7,.09,0x00ff00)));
    REQUIRE(bridge.acquire(Display::Main)==main);
    REQUIRE(bridge.acquire(Display::Scope)->pixels[0]==0x00ff00);
    REQUIRE(bridge.publish(Display::Main,frame(generation,2,.2,0x0000ff)));
    REQUIRE(main->pixels[0]==0xff0000);
    REQUIRE(!bridge.publish(Display::Main,frame(generation,1,.3,0)));
    REQUIRE(!bridge.publish(Display::Main,frame(generation,3,.1,0)));
    auto invalid=frame(generation,3,.3,0);invalid.width=4097;
    REQUIRE(!bridge.publish(Display::Main,std::move(invalid)));
    invalid=frame(generation,3,.3,0);invalid.pixels.pop_back();
    REQUIRE(!bridge.publish(Display::Main,std::move(invalid)));
    invalid=frame(generation,3,.3,0);invalid.aspect=0.0/0.0;
    REQUIRE(!bridge.publish(Display::Main,std::move(invalid)));
    bridge.reset();
    REQUIRE(!bridge.acquire(Display::Main));REQUIRE(!bridge.acquire(Display::Scope));
    REQUIRE(!bridge.publish(Display::Scope,frame(generation,100,10,0)));
    auto next=bridge.generation();REQUIRE(next!=generation);
    std::thread producer([&]{for(uint64_t i=1;i<=1000;++i)bridge.publish(Display::Scope,frame(next,i,i*.01,uint32_t(i)));});
    for(int i=0;i<1000;++i)if(auto snapshot=bridge.acquire(Display::Scope))REQUIRE(snapshot->pixels[0]==snapshot->sequence);
    producer.join();
    REQUIRE(bridge.acquire(Display::Scope)->sequence==1000);
    std::cout << "Display ownership, immutable snapshots, session reset and threaded latest-frame checks passed\n";
}
