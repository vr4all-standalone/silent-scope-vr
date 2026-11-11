// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstring>
#include <unordered_map>
namespace silent_scope::scene {
struct EmulationProfile {bool enabled{};std::unordered_map<const char*,uint64_t> nanoseconds;};
inline EmulationProfile& emulation_profile(){static EmulationProfile profile;return profile;}
inline void configure_emulation_profile(const char* request,double seconds){
 emulation_profile().enabled=request&&std::strcmp(request,"1")==0&&seconds>=15&&seconds<120;
}
class EmulationScope {
 using Clock=std::chrono::steady_clock;const char* tag_;Clock::time_point began_{};
public:
 explicit EmulationScope(const char* tag):tag_(emulation_profile().enabled?tag:nullptr){if(tag_)began_=Clock::now();}
 ~EmulationScope(){if(tag_)emulation_profile().nanoseconds[tag_]+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-began_).count();}
};
}

