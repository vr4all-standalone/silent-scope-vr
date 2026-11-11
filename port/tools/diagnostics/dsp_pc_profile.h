// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
namespace silent_scope::scene {
inline uint64_t* dsp_pc_profile(const char* tag,double seconds){
 static const char* root=std::getenv("SILENT_SCOPE_DSP_PROFILE"); if(!root)return nullptr;
 struct Samples{std::array<uint64_t,65536> counts{};bool written{};};static Samples sample[2];
 int index=std::strcmp(tag,":dsp1")==0?0:std::strcmp(tag,":dsp2")==0?1:-1;if(index<0)return nullptr;
 auto& entry=sample[index];
 if(seconds>=70&&!entry.written){entry.written=true;std::vector<std::pair<uint64_t,int>> sorted;for(int i=0;i<65536;i++)if(entry.counts[i])sorted.emplace_back(entry.counts[i],i+0x20000);std::sort(sorted.rbegin(),sorted.rend());std::ofstream out(std::string(root)+"/dsp"+std::to_string(index+1)+"-pcs.txt");for(auto [count,pc]:sorted)out<<std::hex<<pc<<std::dec<<' '<<count<<'\n';}
 return seconds>=35&&seconds<70?entry.counts.data():nullptr;
}
}
