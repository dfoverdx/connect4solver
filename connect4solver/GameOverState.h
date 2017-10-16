#pragma once
#include "Typedefs.h"

namespace connect4solver {
    enum gameOverState {
        unfinished = 0,
        blackWon = 1,
        blackLost = -1
    };
}