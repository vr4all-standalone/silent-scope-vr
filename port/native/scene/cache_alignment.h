// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <bit>
#include <cstdint>
#include <mutex>
namespace silent_scope::scene {
class CacheAlignment {
    std::once_flag resolved_;
    uintptr_t mask_{63};
public:
    template<class Probe>uintptr_t get(Probe probe){
        std::call_once(resolved_,[&]{auto bytes=uintptr_t(probe());if(!bytes||bytes>(1U<<20))bytes=64;mask_=std::bit_ceil(bytes)-1;});
        return mask_;
    }
};
}
