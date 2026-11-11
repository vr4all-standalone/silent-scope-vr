// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace silent_scope::scene {
// ADSP21062 2M internal maps only. External devices and the short-word SSE
// view deliberately stay on the normal memory dispatcher. Always read live
// SRAM so PM writes, DMA, DM aliases and reset need no cache invalidation.
struct SharcSram {
 const uint32_t *first,*second;
 bool program_read(uint32_t address,uint64_t& value)const{
  if(address<0x20000||address>=0x40000)return false;
  const auto offset=address&0x7fff;
  if(offset>=0x5000)return false;
  const auto* words=address<0x28000?first:second;
  const auto slot=offset>>12,base=(offset&0xfff)+(slot>>1)*0x3000;
  value=slot&1?(uint64_t(words[base+0x2000])<<16)|(words[base+0x1000]&0xffff)
              :(uint64_t(words[base])<<16)|(words[base+0x1000]>>16);
  return true;
 }
 bool data_read(uint32_t address,uint32_t& value)const{
  if(address<0x20000||address>=0x40000)return false;
  value=(address<0x28000?first:second)[address&0x7fff];return true;
 }
};
}
