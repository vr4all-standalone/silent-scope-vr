// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cmath>
#include <cstdint>
namespace silent_scope::scene {
// Inverse of MAME's Voodoo compute_wfloat; use the quantization-bin midpoint.
// The returned value is reciprocal camera-forward depth, like RawVertex::q.
inline double decode_background_w(uint16_t depth) {
    if(depth==0||depth==65535)return 0;
    const unsigned value=depth-1,exponent=value>>12,mantissa=value&4095;
    return std::ldexp(double(8191^mantissa)+.5,-int(13+exponent));
}
}

