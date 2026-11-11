// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "projection.h"
#include <bit>
#include <cstdint>
namespace silent_scope::scene {
// Measured on US UAD1.33 at native35/105/110seconds. Both startup and projection
// microcode signatures must match before reading the SDK's viewport descriptor.
// Main DSP table0x28232: center4096/3072, focal-7088/-7088 in sixteenth pixels.
template<class Program,class Data>
NativeLens read_silent_scope_lens(Program program,Data data,int width,int height){
    if(width!=512||height!=384||program(0x20042)!=0x0f1600028263ULL||
       program(0x20043)!=0x0f1700028220ULL||program(0x20134)!=0x6c3e518da2afULL||
       program(0x21fd2)!=0x6e3e97597038ULL)return {};
    auto number=[&](uint32_t address){return double(std::bit_cast<float>(uint32_t(data(address))));};
    NativeLens lens{width,height,-number(0x28234)/16.,-number(0x28236)/16.,number(0x28232)/16.,number(0x28233)/16.};
    if(!lens.valid()||lens.fx<32||lens.fy<32||lens.fx>8192||lens.fy>8192||
       lens.cx<0||lens.cy<0||lens.cx>=width||lens.cy>=height)return {};
    return lens;
}
}
