// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
namespace silent_scope::scene {
using RasterKey=std::array<uint32_t,7>;
inline const char* raster_profile_path(){static const char* path=std::getenv("SILENT_SCOPE_RASTER_PROFILE");return path;}
struct RasterProfile {
 struct Count {uint64_t triangles{},pixels{},generic_pixels{};};std::map<RasterKey,Count> states;
 ~RasterProfile(){if(const char* path=raster_profile_path()){
  std::ofstream out(path);out<<"generic,fbzcp,alpha,fog,fbz,tex0,tex1,triangles,pixels,generic_pixels\n";
  for(const auto& [key,count]:states){for(auto value:key)out<<value<<',';out<<count.triangles<<','<<count.pixels<<','<<count.generic_pixels<<'\n';}
 }}
};
inline void record_raster(const RasterKey& key,uint32_t pixels,bool generic){
 static RasterProfile profile;auto& count=profile.states[key];++count.triangles;count.pixels+=pixels;if(generic)count.generic_pixels+=pixels;
}
}

