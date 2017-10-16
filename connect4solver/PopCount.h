#pragma once
#include <nmmintrin.h>
#include "Typedefs.h"

namespace connect4solver {
    extern inline int popCountByte(uchar x);
    extern inline const ull popCount64(long long x);
    //const auto& popCount64 = _mm_popcnt_u64;
}