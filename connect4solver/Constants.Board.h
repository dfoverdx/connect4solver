#pragma once
#include <climits>
#include "Constants.h"
#include "Helpers.h"
#include "Typedefs.h"

namespace connect4solver {
    constexpr Heuristic BLACK_WON_HEURISTIC = (std::numeric_limits<Heuristic>::max)();
    constexpr Heuristic BLACK_LOST_HEURISTIC = (std::numeric_limits<Heuristic>::min)() + 1;
    constexpr Heuristic CATS_GAME_HEURISTIC = (std::numeric_limits<Heuristic>::min)();

    constexpr BoardPieces EMPTY_COLUMN_BITS = (1 << BOARD_HEIGHT) - 1;
    constexpr BoardPieces EMPTY_OPEN_SPACES_BITS = RepeatedByte<EMPTY_COLUMN_BITS, MAX_COLUMN>::value;
    constexpr BoardPieces BOARD_MASK = EMPTY_OPEN_SPACES_BITS;

    constexpr int COLUMN_HEIGHT_REQUIRED_BITS = RequiredBits<BOARD_HEIGHT>::value;
    constexpr int SQUISHED_BOARD_COLUMN_BITS = COLUMN_HEIGHT_REQUIRED_BITS + BOARD_HEIGHT;
}