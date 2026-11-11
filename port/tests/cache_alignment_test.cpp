#include "scene/cache_alignment.h"
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
using namespace silent_scope::scene;
int main(){
    CacheAlignment cache;std::atomic<unsigned> probes{};
    std::vector<std::thread> workers;std::atomic<bool> bad{};
    for(int i=0;i<8;++i)workers.emplace_back([&]{for(int n=0;n<100;++n)if(cache.get([&]{++probes;return 64U;})!=63)bad=true;});
    for(auto& worker:workers){worker.join();}if(bad||probes!=1)return 1;
    CacheAlignment non_power,missing;if(non_power.get([]{return 96U;})!=127||missing.get([]{return 0U;})!=63)return 1;
    std::cout<<"One cache-line probe per process cache, including concurrent callers and fallback\n";
}
