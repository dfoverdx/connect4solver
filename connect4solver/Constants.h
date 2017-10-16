#pragma once

#include <chrono>
#include "Typedefs.h"

#define ALWAYS_USE_HEURISTIC
//#define MAP
//#define VERBOSE

namespace connect4solver {
    const int BOARD_WIDTH = 7;
    const int BOARD_HEIGHT = 6;
    const int MAX_COLUMN = BOARD_WIDTH - 1;
    const int SYMMETRIC_BOARD_WIDTH = BOARD_WIDTH / 2 + BOARD_WIDTH % 2;
    const int MAX_SYMMETRIC_COLUMN = SYMMETRIC_BOARD_WIDTH - 1;
    const int SYMMETRIC_MID_COLUMN = SYMMETRIC_BOARD_WIDTH - 1;
    const int MAX_ROW = BOARD_HEIGHT - 1;
    const int BOARD_SIZE = BOARD_WIDTH * BOARD_HEIGHT;

    const int MAX_MOVE_DEPTH = BOARD_SIZE - 2;
    const int BLACK_LOST = BOARD_SIZE;

#ifdef NDEBUG

    const int DEPTH_TO_PRINT_PROGRESS = 14;
    const ull DATA_TO_DELETE = 500000;
    const int ALLOWED_MEMORY_PERCENT = 85;
    const ull CHECK_MEMORY_AFTER_BOARDS_EVALUATED = 5000000;
    const int MAX_THREADS = 4;

    const std::chrono::duration<uint, std::milli> TIME_TO_WAIT_FOR_CVS = std::chrono::milliseconds(10000);

//#ifdef ALWAYS_USE_HEURISTIC
//    const Heuristic THREAD_HEURISTIC_THRESHOLD = 30;
//#else
    const int THREAD_AT_DEPTH = 14;
//#endif

#else

    const int DEPTH_TO_PRINT_PROGRESS = 20;
    const ull DATA_TO_DELETE = 500000;
    const int ALLOWED_MEMORY_PERCENT = 0; // 10;
    const ull CHECK_MEMORY_AFTER_BOARDS_EVALUATED = 10000;
    const int MAX_THREADS = 2;
    const std::chrono::duration<uint, std::milli> TIME_TO_WAIT_FOR_CVS = std::chrono::milliseconds(5000);

//#ifdef ALWAYS_USE_HEURISTIC
//    const Heuristic THREAD_HEURISTIC_THRESHOLD = 20;
//#else
    const int THREAD_AT_DEPTH = 10;
//#endif

#endif
}