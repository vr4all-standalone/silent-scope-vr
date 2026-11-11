#include <iostream>
#include <vector>
#if __has_include("dreamcast/dc_mouse_input_source.h")
#include "dreamcast/dc_mouse_input_source.h"
#define HAS_MOUSE_SOURCE 1
#endif
#define REQUIRE(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";return 1;}}while(false)
int main(){
#ifndef HAS_MOUSE_SOURCE
 REQUIRE(false&&"Native mouse source reader missing");
#else
 using namespace silent_scope::dc_mouse;
 std::vector<std::uint8_t> ram(16u*1024*1024,0);
 const auto w16=[&](std::uint32_t address,std::uint16_t value){const auto p=address&0xffffff;ram[p]=value&255;ram[p+1]=value>>8;};
 const auto w32=[&](std::uint32_t address,std::uint32_t value){w16(address,std::uint16_t(value));w16(address+2,std::uint16_t(value>>16));};
 REQUIRE(!read_source(ram.data(),ram.size(),"OTHER").code_verified);
 REQUIRE(!read_source(nullptr,ram.size(),"T9507N").code_verified);
 REQUIRE(!read_source(ram.data(),ram.size()-1,"T9507N").code_verified);
 for(auto g:MouseWordGuards)w16(g.first,g.second);
 for(auto g:MouseLongGuards)w32(g.first,g.second);
 for(auto g:MouseTableGuards)w32(g.first,g.second);
 for(unsigned i=0;i<MouseClampGuards.size();++i)w16(0x8c073e7eu + 2*i,MouseClampGuards[i]);
 w16(0x8c073ee4,5500);w16(0x8c073ee6,std::uint16_t(-5500));w16(0x8c073ee8,4300);w16(0x8c073eea,std::uint16_t(-4300));
 w32(0x8c1d54ac,0x8c300000);w32(0x8c26fe1c,1);w32(0x8c1b013c,1);ram[0x1b015c]=255;
 w32(0x8c272328,0x100);w32(0x8c272320,0x200);w32(0x8c272324,0x400);
 w16(0x8c1b0862,std::uint16_t(-40));w16(0x8c1b0860,80);
 auto facts=read_source(ram.data(),ram.size(),"T9507N");
 REQUIRE(facts.code_verified&&facts.context.mouse_override&&facts.yaw==-40&&facts.pitch==80);
 REQUIRE(source_hint(facts.context).verified&&source_hint(facts.context).yaw==-2.5);
 w32(0x8c300008,0x200);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(facts.context.alternate_a&&!source_hint(facts.context).verified);
 w32(0x8c300008,0x300);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(!facts.context.alternate_a);
 w32(0x8c300008,0x10);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(facts.context.digital_override&&!source_hint(facts.context).verified);
 w32(0x8c1d54ac,0);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(facts.code_verified&&!facts.context.code_verified);
 w32(0x8c1d54ac,0xcc300000);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(!facts.context.code_verified);
 w32(0x8c1d54ac,0x8c300000);w32(0x8c300008,0);
 w32(MouseTableGuards[0].first,0);facts=read_source(ram.data(),ram.size(),"T9507N");REQUIRE(facts.code_verified&&!source_hint(facts.context).verified);
 w32(MouseTableGuards[0].first,MouseTableGuards[0].second);
 w16(0x8c073ee4,10961);w16(0x8c073ee6,std::uint16_t(-10961));w16(0x8c073ee8,9591);w16(0x8c073eea,std::uint16_t(-9591));
 REQUIRE(!read_source(ram.data(),ram.size(),"T9507N").code_verified);
 REQUIRE(read_source(ram.data(),ram.size(),"T9507N",10961,9591).code_verified);
 REQUIRE(!read_source(ram.data(),ram.size(),"T9507N",10000,9000).code_verified);
 w16(MouseWordGuards[0].first,0);REQUIRE(!read_source(ram.data(),ram.size(),"T9507N",10961,9591).code_verified);
#endif
 return 0;
}
