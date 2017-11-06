#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <time.h>
#include <Windows.h>
#include <Psapi.h> // must go after Windows.h for some reason
#include "CompilerFlags.h"
#include "Constants.h"
#include "IoMessage.h"
#include "Macros.h"
#include "Typedefs.h"

namespace connect4solver {
    namespace gc {
        enum GCRunState {
            stopped,
            running
        };

        //extern std::mutex gcMutex;

        struct GCState {
            GCState(std::vector<IoMessage>* messages) : messages(messages) {}
            GCRunState runState = GCRunState::stopped;
            const std::vector<IoMessage>* messages;

            clock_t gcStartedAt;
            int currentGCDepth = -1;
            size_t minMemoryToClear = 0;

            int nextGCRunCount = 1;
            ull totalGarbageCollected = 0;
            clock_t timeSpentGCing = 0;

            ull lastGarbageCollected = 0;
            clock_t lastGCTime;
            double lastPercentMovesDeleted = 0.0;
            size_t lastMemoryFreed = 0;
            int lastCacheDepthScoured = 0;

            PROCESS_MEMORY_COUNTERS pmc;
            size_t maxMemoryToUse;

            int cacheFullAt = MAX_MOVE_DEPTH;
        };

        extern void initGC();
        extern const GCState& getGCState();

        // returns true if GC was run, else false
        extern bool manageMemory(int depth, const ull movesAddedSinceLastGC, const int lowestDepthEvaluatedSinceLastGC, int &lowestDepthEvaluated);
    }
}