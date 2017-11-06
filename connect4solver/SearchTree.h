#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <time.h>
#include <string>
#include <unordered_set>
#include "Board.h"
#include "CompilerFlags.h"
#include "Constants.h"
#include "GarbageCollector.h"
#include "Macros.h"
#include "MoveCache.h"
#include "MoveData.h"
#include "MoveManager.h"
#include "SearchTree.CompilerFlagIncludes.h"
#include "SearchTree.Macros.h"

namespace connect4solver {
    namespace SearchTree {
        struct SearchTreeState {
            std::clock_t begin_time = std::clock();

            std::atomic<ull> movesEvaluated = 0;
            std::atomic<int> movesFinishedAtDepth[DEPTH_COUNTS_TO_TRACK];
            std::atomic<int> maxMovesAtDepth[DEPTH_COUNTS_TO_TRACK];

#if ET | QS
            std::atomic<ull> movesAddedSinceLastGC = 0;

            std::atomic<int> lowestDepthEvaluated = MAX_MOVE_DEPTH;
            std::atomic<int> lowestDepthEvaluatedSinceLastGC = MAX_MOVE_DEPTH;

            std::array<std::array<int, BOARD_SIZE>, MAX_THREADS + 1> allCurrentMoveRoutes;

            double threadProgress[MAX_THREADS];
            std::atomic<double> progressSaved = 0.0;
            std::bitset<MAX_THREADS> threadFinished;
            std::bitset<MAX_THREADS> threadsAvailable;

            struct {
                std::atomic<ull> hits;
                std::atomic<ull> misses;

                std::atomic<ull> symmetricBoards;
                std::atomic<ull> asymmetricBoards;
            } stats;
#else // ET | QS
            ull movesAddedSinceLastGC = 0;

            int lowestDepthEvaluated = MAX_MOVE_DEPTH;
            int lowestDepthEvaluatedSinceLastGC = MAX_MOVE_DEPTH;

            std::array<int, BOARD_SIZE> currentMoveRoute;

            std::atomic<double> totalProgress = 0.0;
            std::atomic<double> progressSaved = 0.0;

            struct {
                ull cacheHits;
                ull cacheMisses;

                ull symmetricBoards;
                ull possiblySymmetricBoards;
                ull asymmetricBoards;
            } stats;
#endif // ET | QS
        };

        extern void run();
        extern const SearchTreeState& getSearchTreeState();
    }
}