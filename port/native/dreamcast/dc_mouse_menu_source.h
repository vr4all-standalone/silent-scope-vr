// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// US T9507N primary owned1ST_READ SHA2563cf1879f...; code and readback only.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
namespace silent_scope { namespace dc_mouse {
inline constexpr std::array<std::pair<std::uint32_t,std::uint16_t>,77> MenuNavigationWordGuards{{
 {0x8c086a2eu,0xd31cu},{0x8c086a30u,0x6b43u},{0x8c086a32u,0xd21cu},
 {0x8c086a34u,0x4c15u},{0x8c086a36u,0xd11cu},{0x8c086a38u,0x4f22u},
 {0x8c086a3au,0x2342u},{0x8c086a3cu,0x22c2u},{0x8c086a3eu,0x2152u},
 {0x8c01d5e2u,0x53d3u},
 {0x8c01d5e4u,0x8543u},
 {0x8c01d5e6u,0x330cu},
 {0x8c01d5e8u,0x1d33u},
 {0x8c01d60cu,0x53e6u},
 {0x8c01d60eu,0x4315u},
 {0x8c01d610u,0x8b03u},
 {0x8c01d612u,0x51e6u},
 {0x8c01d614u,0x71ffu},
 {0x8c01d616u,0xa006u},
 {0x8c01d618u,0x1e16u},
 {0x8c01d61au,0x52e6u},
 {0x8c01d61cu,0x4211u},
 {0x8c01d61eu,0x8902u},
 {0x8c01d620u,0x51e6u},
 {0x8c01d622u,0x7101u},
 {0x8c01d624u,0x1e16u},
 {0x8c01d626u,0x52e6u},
 {0x8c01d628u,0x53e3u},
 {0x8c01d62au,0x323cu},
 {0x8c01d62cu,0x1e26u},
 {0x8c01d62eu,0x1e43u},
 {0x8c08cb7cu,0x62d2u},
 {0x8c08cb7eu,0x4211u},
 {0x8c08cb80u,0x8b04u},
 {0x8c08cb82u,0x51d6u},
 {0x8c08cb84u,0x4111u},
 {0x8c08cb86u,0x8901u},
 {0x8c08cb88u,0xa004u},
 {0x8c08cb8au,0xe001u},
 {0x8c08cc9cu,0x62d2u},
 {0x8c08cc9eu,0x4211u},
 {0x8c08cca0u,0x8b04u},
 {0x8c08cca2u,0x51d6u},
 {0x8c08cca4u,0x4115u},
 {0x8c08cca6u,0x8b01u},
 {0x8c08cca8u,0xa004u},
 {0x8c08ccaau,0xe001u},
 {0x8c086ca6u,0xd330u},
 {0x8c086ca8u,0xde2eu},
 {0x8c086caau,0xdd2du},
 {0x8c086cacu,0x430bu},
 {0x8c086caeu,0xe401u},
 {0x8c086cb0u,0x2008u},
 {0x8c086cb2u,0x890cu},
 {0x8c086cb4u,0x63e2u},
 {0x8c086cb6u,0x2338u},
 {0x8c086cb8u,0x8904u},
 {0x8c086cbau,0x63e2u},
 {0x8c086cbcu,0x73ffu},
 {0x8c086cbeu,0x2e32u},
 {0x8c086cceu,0xd227u},
 {0x8c086cd0u,0x420bu},
 {0x8c086cd2u,0xe401u},
 {0x8c086cd4u,0x2008u},
 {0x8c086cd6u,0x890du},
 {0x8c086cd8u,0xd325u},
 {0x8c086cdau,0x61e2u},
 {0x8c086cdcu,0x6232u},
 {0x8c086cdeu,0x72ffu},
 {0x8c086ce0u,0x3123u},
 {0x8c086ce2u,0x8904u},
 {0x8c086ce4u,0x60e2u},
 {0x8c086ce6u,0x7001u},
 {0x8c086ce8u,0x2e02u},
 {0x8c0861e6u,0xd338u},
 {0x8c0861e8u,0x430bu},
 {0x8c0861eau,0x0009u},
}};
inline constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,16> MenuNavigationLongGuards{{
 {0x8c086aa0u,0x8c272034u},{0x8c086aa4u,0x8c272038u},{0x8c086aa8u,0x8c27203cu},{0x8c0860c4u,0x8c0e1a48u},
 {0x8c0e1a48u,0x8c0e1a04u},{0x8c0e1a4cu,0x8c0e1a10u},{0x8c0e1a50u,0x8c0e1a1eu},{0x8c0e1a54u,0x8c0e1a26u},
 {0x8c01d680u,0x8c1e1d68u},
 {0x8c08cc28u,0x8c1e1d68u},
 {0x8c08cd48u,0x8c1e1d68u},
 {0x8c086d64u,0x8c272034u},
 {0x8c086d68u,0x8c08cb20u},
 {0x8c086d6cu,0x8c08cc40u},
 {0x8c086d70u,0x8c272038u},
 {0x8c0862c8u,0x8c086ca0u},
}};
struct MenuSourceFacts {bool code_verified{},mouse_present{},main_menu_verified{};int selection{},count{},wheel{};unsigned main_menu_state{};std::uint32_t table_pointer{};};
inline MenuSourceFacts read_menu_source(const std::uint8_t* ram,std::size_t bytes,std::string_view title)noexcept{
 MenuSourceFacts out;if(!ram||bytes!=16u*1024*1024||title!="T9507N")return out;
 const auto r16=[&](std::uint32_t a){a&=0xffffff;return std::uint16_t(ram[a]|(std::uint16_t(ram[a+1])<<8));};
 const auto r32=[&](std::uint32_t a){return std::uint32_t(r16(a))|(std::uint32_t(r16(a+2))<<16);};
 const auto signed32=[](std::uint32_t n){return int(n<0x80000000u?std::int64_t(n):std::int64_t(n)-0x100000000ll);};
 for(auto g:MenuNavigationWordGuards)if(r16(g.first)!=g.second)return out;
 for(auto g:MenuNavigationLongGuards)if(r32(g.first)!=g.second)return out;
 out.code_verified=true;out.mouse_present=(r32(0x8c1e1d68)&0x80000000u)==0;
 out.selection=signed32(r32(0x8c272034));out.count=signed32(r32(0x8c272038));
 out.wheel=signed32(r32(0x8c1e1d80));out.main_menu_state=ram[0x1b014d];out.table_pointer=r32(0x8c27203c);
 out.main_menu_verified=out.mouse_present&&out.main_menu_state==3&&out.table_pointer==0x8c0e1a48u
    &&out.count==4&&out.selection>=0&&out.selection<4;
 return out;
}
}}
