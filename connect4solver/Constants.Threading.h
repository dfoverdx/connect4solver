#pragma once
#include "CompilerFlags.h"
#include "Macros.h"
#include "Typedefs.h"

namespace connect4solver {
#if ENABLE_THREADING
#include <chrono>
#include <thread>

#if DEBUG_THREADING
    constexpr int MAX_CHILD_THREADS = DEBUG_THREADING;
    constexpr int NUM_CORES = 1;
#else // DEBUG_THREADING
    const int NUM_CORES = std::thread::hardware_concurrency(); // may return 0 if function is unavailable

#if QUEUE_SEARCH
    constexpr int DEFAULT_NUM_THREADS = 4;
    const int MAX_CHILD_THREADS = NUM_CORES ? NUM_CORES : DEFAULT_NUM_THREADS;
#else // QUEUE_SEARCH
    constexpr int MAX_CHILD_THREADS = dbgVal(2, 4);
#endif // QUEUE_SEARCH
#endif // DEBUG_THREADING
#if DEPTH_FIRST_SEARCH
    constexpr std::chrono::duration<uint> TIME_TO_WAIR_FOR_CVS = std::chrono::seconds(dbgVal(10, 1));
    constexpr int THREAD_AT_DEPTH = dbgVal(19, 13);
#endif // DEPTH_FIRST_SEARCH
#else // ENABLE_THREADING
    constexpr int MAX_CHILD_THREADS = 0;
#endif // ENABLE_THREADING
}