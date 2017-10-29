#include "PopCount.h"

namespace connect4solver {
    inline const ull popCount64(long long x) {
        return _mm_popcnt_u64(x);
    }
}