#include "scene/sharc_sram.h"
#include <array>
#include <iostream>
using silent_scope::scene::SharcSram;
int main(){
 std::array<uint32_t,32768> first{},second{};
 for(size_t i=0;i<first.size();++i){first[i]=uint32_t(i*0x179173u+0x89124567u);second[i]=~first[i];}
 SharcSram ram{first.data(),second.data()};uint64_t program{};uint32_t data{};
 for(uint32_t address=0x20000;address<0x40000;++address){
  const auto& block=address<0x28000?first:second;
  if(!ram.data_read(address,data)||data!=block[address%32768])return 1;
  // Reference is the four independently mapped 20K program windows.
  auto offset=(address-0x20000)%32768;
  if(offset>=20480){if(ram.program_read(address,program))return 2;continue;}
  const auto& words=address<0x28000?first:second;
  uint32_t group=offset/8192,word=offset%4096;
  uint64_t expected=uint64_t(words[group*12288+(offset%8192<4096?word:word+8192)])<<16;
  expected|=offset%8192<4096?words[group*12288+word+4096]>>16:words[group*12288+word+4096]&65535;
  if(!ram.program_read(address,program)||program!=expected)return 3;
 }
 for(uint32_t address:{0u,0x1ffffu,0x40000u,0x7ffffu,0x400000u,0xffffffffu})
  if(ram.program_read(address,program)||ram.data_read(address,data))return 4;
 // No decoded opcode cache: DMA/DM alias changes appear on the next fetch.
 ram.program_read(0x20000,program);auto old=program;first[0]^=0xa5479231;
 if(!ram.program_read(0x20000,program)||program==old)return 5;
 ram.program_read(0x28000,program);old=program;second[4096]^=0x12340000;
 if(!ram.program_read(0x30000,program)||program==old)return 6;
 std::cout<<"All 2M SRAM read aliases, holes, packing and live writes passed\n";
}
