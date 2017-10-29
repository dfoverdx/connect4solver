#pragma once
#include "CompilerFlags.h"
#include "Typedefs.h"

namespace connect4solver {
#if ENABLE_THREADING

#include <chrono>

#ifdef NDEBUG
    constexpr int MAX_CHILD_THREADS = 4;
    constexpr std::chrono::duration<uint, std::milli> TIME_TO_WAIT_FOR_CVS = std::chrono::milliseconds(10000);
    constexpr int THREAD_AT_DEPTH = 13; // THREAD_DEPTH currently must be ODD so that it lands on black's turn
#else
    constexpr int MAX_CHILD_THREADS = 2;
    constexpr std::chrono::duration<uint, std::milli> TIME_TO_WAIT_FOR_CVS = std::chrono::milliseconds(1000);
    constexpr int THREAD_AT_DEPTH = 19; // THREAD_DEPTH currently must be ODD so that it lands on black's turn
#endif
#else
    constexpr int MAX_CHILD_THREADS = 0;
#endif
}