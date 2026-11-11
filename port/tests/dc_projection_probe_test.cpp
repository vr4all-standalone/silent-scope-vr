#include "dreamcast/dc_projection_probe.h"
#include <cassert>
#include <map>
using namespace silent_scope::dreamcast;
static uint32_t bits(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
int main(){
    std::map<uint32_t,uint32_t> ram;const uint32_t header=0x8c100000;
    ram[header]=4;ram[header+4]=0x8ce3fea0;ram[header+16]=ram[header+20]=bits(555);
    ram[LensSdkBase]=bits(185); // actual header555 must win over current SDK185
    ram[LensSdkBase+12]=bits(320);ram[LensSdkBase+16]=bits(240);
    ram[LensSourceMain]=bits(555);ram[LensMode]=0;
    auto read=[&](uint32_t a){return ram.at(a);};
    auto p=read_projection_entry(0x8c0fbe28,header,0,read);
    assert(p.linear&&p.candidate&&p.begin==0xe3fea0&&p.length==256&&p.fx==555&&p.fy==555);
    p=read_projection_entry(0x8c0fbe28,header,1u<<20,read);assert(!p.candidate);
    ram[header+4]=0x10000000;p=read_projection_entry(0x8c0fbe28,header,0,read);assert(!p.linear);
    ram[header+4]=0x8cffffe0;p=read_projection_entry(0x8c0fbe28,header,0,read);assert(!p.linear);
    ram[header+4]=0x8ce3fea0;ram[LensMode]=1;p=read_projection_entry(0x8c0fbe28,header,0,read);assert(p.linear&&!p.candidate);
    ram[LensMode]=0;
    for(auto offset:{0x14u,0x1cu,0x24u,0x28u,0x2cu,0x30u})ram[0x8cd206b0+offset]=0;
    ram[0x8cd206c4]=8;ram[0x8cd206cc]=0x8cf1ff20;
    ram[0x8cd206d4]=ram[0x8cd206d8]=bits(185);ram[0x8cd206dc]=bits(320);ram[0x8cd206e0]=bits(240);
    p=read_projection_entry(0x8c112566,0,0,read);assert(p.linear&&p.candidate&&p.length==256&&p.fx==185);
    p=read_projection_entry(0x8c124dc4,0,0,read);assert(!p.linear&&!p.candidate); // scatter kernel needs individual writes
    assert(projection_site(0x8c112566)==6&&projection_site(0x8c124dc4)==7&&projection_site(0x8c010000)==-1);
}
