#pragma once
#include "Constants.h"
#include "Constants.Board.h"
#include "Constants.Threading.h"

namespace connect4solver {
    namespace {
        static_assert(BOARD_WIDTH * SQUISHED_BOARD_COLUMN_BITS <= 64, "Board::calcBoardHash() can only handle sizes where the represenation of the entire board fits in 64 bits.  If this is an issue look into using boost and its int128_t.");

#if ENABLE_THREADING
        static_assert((THREAD_AT_DEPTH % 2) == 1, "THREAD_AT_DEPTH must be odd--this restriction should be relaxed");
        static_assert(MAX_CHILD_THREADS > 1, "MAX_CHILD_THREADS must be at least 2");
        static_assert(MAX_CHILD_THREADS <= BOARD_WIDTH, "MAX_CHILD_THREADS must be at most the width of the board (7)");
        static_assert(DEPTH_COUNTS_TO_TRACK < THREAD_AT_DEPTH, "Can only track depths up through the depth that threading begins.");
#else
        static_assert(MAX_CHILD_THREADS == 0, "MAX_CHILD_THREADS must be 0 for non-threaded settings");
#endif
    }
}