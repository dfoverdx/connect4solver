#pragma once
#include "Typedefs.h"

namespace connect4solver {
    enum piece {
        empty = -1,
        black = 0,
        red = 1,
    };

    const piece EMPTY = piece::empty;
    const piece BLACK = piece::black;
    const piece RED = piece::red;
}