#include "scene/game_camera.h"
#include <bit>
#include <iostream>
#include <map>
using namespace silent_scope::scene;
int main(){
    std::map<uint32_t,uint64_t> code{{0x20042,0x0f1600028263ULL},{0x20043,0x0f1700028220ULL},{0x20134,0x6c3e518da2afULL},{0x21fd2,0x6e3e97597038ULL}};
    std::map<uint32_t,uint32_t> data;
    for(auto [address,value]:std::initializer_list<std::pair<uint32_t,float>>{{0x28232,4096},{0x28233,3072},{0x28234,-7088},{0x28236,-7088}})data[address]=std::bit_cast<uint32_t>(value);
    auto read=[&]{return read_silent_scope_lens([&](uint32_t a){return code[a];},[&](uint32_t a){return data[a];},512,384);};
    auto lens=read();if(!lens.valid()||lens.fx!=443||lens.fy!=443||lens.cx!=256||lens.cy!=192)return 1;
    auto point=lens.unproject(300,220,4);auto projected=lens.project_point(point);
    if(!projected.valid||std::abs(projected.u-300./511)>1e-8||std::abs(projected.v-220./383)>1e-8)return 1;
    code[0x20134]=0;if(read().valid())return 1;
    code[0x20134]=0x6c3e518da2afULL;data[0x28234]=std::bit_cast<uint32_t>(NAN);if(read().valid())return 1;
    std::cout<<"Measured Silent Scope camera, projection round trip and unknown-code guards passed\n";
}
