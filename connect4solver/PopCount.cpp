#include <array>
#include "PopCount.h"

namespace connect4solver {
    using namespace std;

    const int UCHAR_DISTINCT_VALUES = 256;

    const array<int, UCHAR_DISTINCT_VALUES> generateOneBitsArray() {
        const array<int, 16> oneBits = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
        
        array<int, UCHAR_DISTINCT_VALUES> vals;
        for (int i = 0; i < UCHAR_DISTINCT_VALUES; ++i) {
            vals[i] = oneBits[i & 0x0F] + oneBits[i >> 4];
        }

        return vals;
    }

    const array<int, UCHAR_DISTINCT_VALUES> oneBits = generateOneBitsArray();

    inline int popCountByte(uchar x)
    {
        return oneBits[x];
    }

    inline const ull popCount64(long long x) {
        return _mm_popcnt_u64(x);
    }
}