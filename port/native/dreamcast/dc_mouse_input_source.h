// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Read-only US T9507N input provenance: owned1ST_READ SHA2563cf1879f... .
// No native memory writes or speculative pointer/absolute-gun conversion.
#include "dc_mouse_input.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
namespace silent_scope { namespace dc_mouse {
inline constexpr std::array<std::pair<std::uint32_t,std::uint16_t>,71> MouseWordGuards{{
    {0x8c01b4deu,0x332cu},
    {0x8c01b4ecu,0x7560u},
    {0x8c01b4f2u,0x6551u},
    {0x8c01b4f4u,0x8531u},
    {0x8c01b4f6u,0x420bu},
    {0x8c073b9cu,0xde21u},
    {0x8c073cdcu,0x035eu},
    {0x8c073ce0u,0x8d01u},
    {0x8c073ce2u,0xf44du},
    {0x8c073ce4u,0xff42u},
    {0x8c073ce8u,0x045eu},
    {0x8c073cecu,0x8d03u},
    {0x8c073cf8u,0xf546u},
    {0x8c073cfcu,0xfe52u},
    {0x8c073cfeu,0x6000u},
    {0x8c073d00u,0xc810u},
    {0x8c073d02u,0x8d01u},
    {0x8c073d04u,0xff52u},
    {0x8c073d06u,0xfe42u},
    {0x8c073d08u,0xd215u},
    {0x8c073d12u,0x8b35u},
    {0x8c073d80u,0xd117u},
    {0x8c073da8u,0xd210u},
    {0x8c073dd2u,0x8915u},
    {0x8c073e00u,0xd43au},
    {0x8c073e06u,0xf34du},
    {0x8c073e08u,0xfb3au},
    {0x8c073e10u,0xfd2au},
    {0x8c073e14u,0x6320u},
    {0x8c073e16u,0x4311u},
    {0x8c073e18u,0x8931u},
    {0x8c073e22u,0x8802u},
    {0x8c073e24u,0x8f20u},
    {0x8c073e6eu,0x0e15u},
    {0x8c073e7cu,0x0e15u},
    {0x8c073ee0u,0x06feu},
    {0x8c073ee2u,0x06fcu},
    {0x8c08bf42u,0xb2ebu},
    {0x8c08bf48u,0x8b09u},
    {0x8c08bf4eu,0xb2e5u},
    {0x8c08bf54u,0x8903u},
    {0x8c08bf70u,0xb2d4u},
    {0x8c08bf76u,0x8b23u},
    {0x8c08bf7cu,0xb2ceu},
    {0x8c08bf82u,0x891du},
    {0x8c08c534u,0x5232u},
    {0x8c08c536u,0x2258u},
    {0x8c08c54cu,0xd51du},
    {0x8c08c550u,0x4311u},
    {0x8c08c552u,0x8b0au},
    {0x8c08c554u,0x5154u},
    {0x8c08c558u,0x8907u},
    {0x8c08c55au,0x5254u},
    {0x8c08c568u,0xf023u},
    {0x8c08c5e0u,0xd517u},
    {0x8c08c5e4u,0x4311u},
    {0x8c08c5e6u,0x8b0au},
    {0x8c08c5e8u,0x5155u},
    {0x8c08c5ecu,0x8907u},
    {0x8c08c5eeu,0x5255u},
    {0x8c08c5fcu,0xf023u},
    {0x8c08cad8u,0x5232u},
    {0x8c08cadau,0x2268u},
    {0x8c08cbd8u,0xe620u},
    {0x8c08cbecu,0x5232u},
    {0x8c08cbeeu,0x2268u},
    {0x8c08cd10u,0x5232u},
    {0x8c08cd12u,0x2278u},
    {0x8c08ce32u,0xe640u},
    {0x8c08ce46u,0x5232u},
    {0x8c08ce48u,0x2268u}
}};
inline constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,24> MouseLongGuards{{
    {0x8c073c24u,0x8c1b0164u},
    {0x8c073c28u,0x8c1b0884u},
    {0x8c073d48u,0x8c26fdc8u},
    {0x8c073d4cu,0x8c0c7a94u},
    {0x8c073d50u,0x8c0c7a88u},
    {0x8c073d54u,0x8c26fdc7u},
    {0x8c073d60u,0x8c1b013cu},
    {0x8c073de0u,0x8c08bf3au},
    {0x8c073decu,0x8c08bf68u},
    {0x8c073eecu,0x8c0c7a64u},
    {0x8c073ef0u,0x8c1b015cu},
    {0x8c073ef4u,0x8c271ffcu},
    {0x8c08bfa0u,0x8c1d54acu},
    {0x8c08bfa4u,0x8c1e1d68u},
    {0x8c08bfacu,0x8c272328u},
    {0x8c08bfb0u,0x8c272320u},
    {0x8c08bfb4u,0x8c272324u},
    {0x8c08c5c0u,0x8c1d54acu},
    {0x8c08c5c4u,0x8c1e1d68u},
    {0x8c08c640u,0x8c1e1d68u},
    {0x8c08cb00u,0x8c1d54acu},
    {0x8c08cc20u,0x8c1d54acu},
    {0x8c08cd40u,0x8c1d54acu},
    {0x8c08cf20u,0x8c1d54acu}
}};
inline constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,9> MouseTableGuards{{
    {0x8c08c5c8u,0x41800000u},
    {0x8c08c644u,0x41800000u},
    {0x8c0c7a64u,0x42200000u},
    {0x8c0c7a88u,0x3f000000u},
    {0x8c0c7a8cu,0x3f800000u},
    {0x8c0c7a90u,0x40000000u},
    {0x8c0c7a94u,0x3f000000u},
    {0x8c0c7a98u,0x3f800000u},
    {0x8c0c7a9cu,0x40000000u}
}};
inline constexpr std::array<std::uint16_t,23> MouseClampGuards{{0x902f,0x9430,0x3ed,0x3347,0x8b00,0xe45,0x3ed,0x942b,0x3343,0x8900,0xe45,0x9025,0x9427,0x3ed,0x3347,0x8b00,0xe45,0x3ed,0x9422,0x3343,0x8900,0xe45,0x4f26}};
struct SourceFacts {
 bool code_verified{};SourceContext context{};double yaw{},pitch{};int yaw_limit{},pitch_limit{};
 std::uint32_t peripheral_address{},buttons{};
};
inline SourceFacts read_source(const std::uint8_t* ram,std::size_t bytes,std::string_view title,int yaw_limit=5500,int pitch_limit=4300)noexcept {
 SourceFacts out;if(!ram||bytes!=16u*1024*1024||title!="T9507N")return out;
 if(!((yaw_limit==5500&&pitch_limit==4300)||(yaw_limit==10961&&pitch_limit==9591)))return out;
 const auto r8=[&](std::uint32_t a){return ram[a&0xffffff];};
 const auto r16=[&](std::uint32_t a){return std::uint16_t(r8(a)|(std::uint16_t(r8(a+1))<<8));};
 const auto r32=[&](std::uint32_t a){return std::uint32_t(r16(a))|(std::uint32_t(r16(a+2))<<16);};
 const auto s16=[&](std::uint32_t a){const int value=r16(a);return value<32768?value:value-65536;};
 for(auto g:MouseWordGuards)if(r16(g.first)!=g.second)return out;
 for(auto g:MouseLongGuards)if(r32(g.first)!=g.second)return out;
 for(unsigned i=0;i<MouseClampGuards.size();++i)if(r16(0x8c073e7eu+2*i)!=MouseClampGuards[i])return out;
 if(s16(0x8c073ee4)!=yaw_limit||s16(0x8c073ee6)!=-yaw_limit||s16(0x8c073ee8)!=pitch_limit||s16(0x8c073eea)!=-pitch_limit)return out;
 out.code_verified=true;out.yaw=s16(0x8c1b0862);out.pitch=s16(0x8c1b0860);out.yaw_limit=yaw_limit;out.pitch_limit=pitch_limit;
 bool tables=true;for(auto g:MouseTableGuards)tables=tables&&r32(g.first)==g.second;
 auto& c=out.context;const auto ptr=r32(0x8c1d54ac);const auto physical=ptr&0x1fffffff;
 out.peripheral_address=ptr;
 const auto area=ptr&0xe0000000u;
 if((area!=0&&area!=0x80000000u&&area!=0xa0000000u)||ptr%4||physical<0x0c000000||physical>0x0cffffdcu)return out;
 const auto buttons=r32(ptr+8),gate=r32(0x8c272328),a=r32(0x8c272320),b=r32(0x8c272324);
 out.buttons=buttons;c.code_verified=tables;
 const auto mouse_type=r32(0x8c1e1d68);c.mouse_override=(mouse_type&0x80000000u)==0;
 c.digital_override=(buttons&0xf0)!=0;
 c.alternate_a=(buttons&gate)==0&&(buttons&a)!=0;
 c.alternate_b=(buttons&gate)==0&&(buttons&b)!=0;
 c.speed_index=r32(0x8c26fe1c);c.reverse_y=r32(0x8c26fe18)!=0;c.reverse_x=(r8(0x8c26fdc7)&0x10)!=0;
 c.mode=r32(0x8c271ffc);c.scope_branch=r32(0x8c1b013c);
 const int sign=r8(0x8c1b015c);c.input_sign=sign<128?sign:sign-256;
 return out;
}
}}
