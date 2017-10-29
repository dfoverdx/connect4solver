#pragma once

#include <chrono>
#include <type_traits>
#include "CompilerFlags.h"
#include "Constants.ErrorMessages.h"
#include "Constants.Threading.h"
#include "Typedefs.h"

namespace connect4solver {
    using namespace std::chrono_literals;

    constexpr int BOARD_WIDTH = 7;
    constexpr int BOARD_HEIGHT = 6;
    constexpr int MAX_COLUMN = BOARD_WIDTH - 1;
    constexpr int SYMMETRIC_BOARD_WIDTH = BOARD_WIDTH / 2 + BOARD_WIDTH % 2;
    constexpr int MAX_SYMMETRIC_COLUMN = SYMMETRIC_BOARD_WIDTH - 1;
    constexpr int MAX_ROW = BOARD_HEIGHT - 1;
    constexpr int BOARD_SIZE = BOARD_WIDTH * BOARD_HEIGHT;

    constexpr int MAX_MOVE_DEPTH = BOARD_SIZE - 2;
    constexpr int LAST_MOVE_DEPTH = MAX_MOVE_DEPTH - 1;
    constexpr int BLACK_LOST = BOARD_SIZE;

    constexpr std::chrono::seconds CHECK_MEMORY_INTERAVAL = 3s;
    constexpr std::chrono::milliseconds PRINT_PROGRESS_INTERVAL = 500ms;

#ifdef NDEBUG
    constexpr int DEPTH_COUNTS_TO_TRACK = 16;
    constexpr int ALLOWED_MEMORY_PERCENT = 80;
    constexpr int STATIC_MEMORY_REMAINING_LIMIT = 500; // MB
    constexpr ull MIN_GARBAGE_TO_DELETE = 10000000;
#else
    constexpr int DEPTH_COUNTS_TO_TRACK = 22;
    constexpr int ALLOWED_MEMORY_PERCENT = 2; // 80;
    constexpr int STATIC_MEMORY_LIMIT = 100; // MB
    constexpr ull MIN_GARBAGE_TO_DELETE = 10000000;
#endif

    constexpr int MAX_THREADS = MAX_CHILD_THREADS + 1;
}