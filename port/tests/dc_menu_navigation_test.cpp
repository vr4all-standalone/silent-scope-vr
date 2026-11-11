#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <fstream>
#include <iterator>
#include <vector>
#if __has_include("dreamcast/dc_menu_navigation.h")
#include "dreamcast/dc_menu_navigation.h"
#include "dreamcast/dc_mouse_menu_probe.h"
#define HAS_MENU_NAVIGATION 1
#endif
#define REQUIRE(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(int argc,char** argv){
#ifndef HAS_MENU_NAVIGATION
 REQUIRE(false&&"Native menu navigation policy missing");
#else
 using silent_scope::dc_mouse::MenuNavigation;
 MenuNavigation navigation;
 REQUIRE(navigation.poll(1,1,true,1)==0); // Held through entry has no neutral rearm.
 REQUIRE(navigation.poll(1.01,0,false,0)==0);
 REQUIRE(navigation.poll(1.02,1,true,1)==1);
 REQUIRE(navigation.poll(1.36,1,false,0)==0);
 REQUIRE(navigation.poll(1.371,1,false,0)==1); // Initial350ms.
 REQUIRE(navigation.poll(1.51,1,false,0)==0);
 REQUIRE(navigation.poll(1.522,1,false,0)==1); // Then150ms.
 REQUIRE(navigation.poll(2,1,false,0)==1); // A stall emits one notch, no catch-up burst.
 REQUIRE(navigation.poll(2.001,1,false,0)==0);
 REQUIRE(navigation.poll(2.01,-1,true,-1)==-1); // Direction reversal is one fresh event.
 REQUIRE(navigation.poll(2.02,0,false,0)==0);
 REQUIRE(navigation.poll(2.5,0,false,0)==0);
 REQUIRE(navigation.poll(2.51,0,true,-1)==-1); // A short stick tap between native polls is retained.
 REQUIRE(navigation.poll(2.52,0,false,0)==0);
 navigation.reset();REQUIRE(navigation.poll(3,-1,true,-1)==0);
 REQUIRE(navigation.poll(3.1,0,false,0)==0);
 REQUIRE(navigation.poll(3.2,-1,true,-1)==-1);
 REQUIRE(navigation.poll(3.3,2,false,0)==0); // Invalid direction cancels/rearms only after neutral.
 REQUIRE(navigation.poll(3.4,-1,true,-1)==0);
 REQUIRE(navigation.poll(std::numeric_limits<double>::quiet_NaN(),0,false,0)==0);
 REQUIRE(navigation.poll(3.5,0,false,0)==0);
 REQUIRE(navigation.poll(3.6,1,true,1)==1);
 REQUIRE(navigation.poll(3.4,1,false,0)==0); // Reverse clock cancels held repeat.
 // Optional owned-code source attestation, not a synthetic emulator test.
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);REQUIRE(input.good());
  std::vector<std::uint8_t> code((std::istreambuf_iterator<char>(input)),{}),ram(16u*1024*1024);
  REQUIRE(code.size()==1386016);std::copy(code.begin(),code.end(),ram.begin()+0x10000);
  const auto w32=[&](std::uint32_t a,std::uint32_t n){a&=0xffffff;for(unsigned i=0;i<4;++i)ram[a+i]=std::uint8_t(n>>(8*i));};
  w32(0x8c272034,2);w32(0x8c272038,5);w32(0x8c1e1d80,0xffffffffu);ram[0x1b014d]=3;
  auto facts=silent_scope::dc_mouse::read_menu_source(ram.data(),ram.size(),"T9507N");
  REQUIRE(facts.code_verified&&facts.mouse_present&&facts.selection==2&&facts.count==5&&facts.wheel==-1&&facts.main_menu_state==3);
  w32(0x8c272038,4);w32(0x8c27203c,0x8c0e1a48);
  REQUIRE(silent_scope::dc_mouse::read_menu_source(ram.data(),ram.size(),"T9507N").main_menu_verified);
  w32(0x8c27203c,0x8c0e1a5c);REQUIRE(!silent_scope::dc_mouse::read_menu_source(ram.data(),ram.size(),"T9507N").main_menu_verified);
  w32(0x8c27203c,0x8c0e1a48);
  REQUIRE(!silent_scope::dc_mouse::read_menu_source(ram.data(),ram.size(),"OTHER").code_verified);
  silent_scope::dc_menu_probe::Policy probe;
  auto sample=probe.poll("T9507N",100,ram.data(),ram.size());REQUIRE(sample.owns&&sample.pressed==silent_scope::dc_mouse::Left);
  sample=probe.poll("T9507N",120,ram.data(),ram.size());REQUIRE(sample.pressed==silent_scope::dc_mouse::Middle&&!sample.wheel);
  sample=probe.poll("T9507N",123,ram.data(),ram.size());REQUIRE(sample.wheel==1);
  sample=probe.poll("T9507N",123.01,ram.data(),ram.size());REQUIRE(!sample.wheel);
  REQUIRE(probe.poll("T9507N",125,ram.data(),ram.size()).wheel==1);
  REQUIRE(probe.poll("T9507N",127,ram.data(),ram.size()).wheel==-1);
  REQUIRE(probe.poll("T9507N",129,ram.data(),ram.size()).wheel==-1);
  REQUIRE(!probe.poll("T9507N",140,ram.data(),ram.size()).wheel);
  ram[0x8cb86]^=1;REQUIRE(!silent_scope::dc_mouse::read_menu_source(ram.data(),ram.size(),"T9507N").code_verified);
 }
#endif
}
