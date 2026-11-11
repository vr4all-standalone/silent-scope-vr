// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <utility>
namespace silent_scope {
enum class OperatorState { Unknown, Normal, Service };
// US UAD1.33 native dispatcher. These are physical PPC AS_PROGRAM addresses,
// independently observed before entry, in Service and after native GAME MODE.
inline constexpr std::array<std::pair<uint32_t,uint32_t>,23> OperatorCode{{
    {0x26c40,0x83e20014u},{0x26c48,0x88bf001au},{0x26c58,0x2f050003u},
    {0x26c5c,0x2c850001u},{0x26c60,0x2f850002u},{0x26c7c,0x419e0058u},
    {0x26ccc,0x987f001au},{0x26cd4,0x4bffeea9u},{0x26cdc,0x38600003u},
    {0x26ce4,0x987f001au},{0x26cf4,0x4bffecb5u},{0x26d94,0x981f001au},
    {0x259b4,0x83820014u},{0x259bc,0x889c002au},{0x259d0,0x30a6fff2u},
    {0x259d8,0x7ca50034u},{0x259fc,0x54bfd97eu},{0x25a30,0x808200b4u},
    {0x25a44,0x9bc40010u},{0x25a90,0x38c00001u},{0x25a94,0x40820018u},
    {0x25aac,0x808200b4u},{0x25ab0,0x98c40010u}}};
template<class Word,class Byte> OperatorState read_operator_state(Word word,Byte byte){
    for(auto [address,value]:OperatorCode)if(word(address)!=value)return OperatorState::Unknown;
    if(word(0xada70)!=0x800ae378u||word(0xadb10)!=0x800b3a30u)return OperatorState::Unknown;
    const auto stage=byte(0xae392),mode=byte(0xae3a2),game=byte(0xb3a40);
    // Stage3 also executes operator submenus. Only mode14 with the native
    // game flag is normal gameplay; Gun Check/Gun Adjust remain Service.
    if(stage==2&&(mode<=11||mode==14)&&game==0)return OperatorState::Service;
    if(stage==3&&mode==14&&game==1)return OperatorState::Normal;
    if(stage==3&&mode<=11&&game==0)return OperatorState::Service;
    return OperatorState::Unknown;
}
}
