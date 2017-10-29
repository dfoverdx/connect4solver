#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <time.h>
#include <unordered_set>
#include <Windows.h>
#include <Psapi.h> // must go after Windows.h for some reason
#include "Board.h"
#include "CompilerFlags.h"
#include "Constants.h"
#include "Macros.h"
#include "MoveData.h"
#include "MoveManager.h"
#include "SearchTree.CompilerFlagIncludes.h"
#include "SearchTree.Macros.h"

#include <iostream>

namespace connect4solver {
    typedef cacheMap<BoardHash, MovePtr> MoveCache;

#if SGC
    typedef std::unordered_set<BoardHash> Garbage;
#endif
    
    class SearchTree
    {
        friend class LowestDepthManager;

    private:
        SearchTree() {
        };

        enum continueEval {
            unfinishedEval,
            finished,
            finishedAfterCheckingNewBoards
        };

        template<typename CurMPtr>
        static bool search(const int depth, const uint threadId, const Board &board, double progress,
            const double progressChunk, CurMPtr &curMovePtr);

        template<typename CurMPtr>
        inline static bool doSearchLast(const int depth, const Board &board, CurMPtr &curMovePtr);

        static bool searchLast(const Board &board, BlackMovePtr &curMovePtr);

        template<typename CurMPtr, class MM>
        inline static bool enumerateBoards(const int depth, const int, const Board &board, MM &mManager,
            Board(&nextBoards)[BOARD_WIDTH], int &invalidMoves, int &finishedMoves);

        template<class MM>
        inline static continueEval doMoveEvaluation(MM &mManager, const int i);

        template<class MM>
        inline static bool doHeuristicEvaluation(MM &mManager, const Board &board, const int i, int &finishedMoves);

        static ull garbageCollect(const int depth, size_t minMemoryToClear);
        static void manageMemory(int depth);

        static void decRedNextRefs(RedMovePtr &ptr, const int depth);

        static void printStatus();
        static void printMoveRoute(std::array<int, BOARD_SIZE> &route);
        static void setCheckMemoryAtInterval();
        static void setMaxMemoryToUse();

        static struct SearchTreeStatus {
            std::clock_t begin_time = std::clock();

            std::atomic<ull> movesEvaluated = 0;
            double lastProgressPerDay = 0;
            std::atomic<int> movesFinishedAtDepth[DEPTH_COUNTS_TO_TRACK];
            std::atomic<int> maxMovesAtDepth[DEPTH_COUNTS_TO_TRACK];

            PROCESS_MEMORY_COUNTERS pmc;
            size_t maxMemoryToUse;

            struct {
                int nextGCCount = 1;
                clock_t timeSpentGCing = 0;
                ull totalGarbageCollected = 0;

                int cacheFullAt = MAX_MOVE_DEPTH;

                ull lastGarbageCollected = 0;
                clock_t lastGCTime;
                double lastPercentMovesDeleted = 0.0;
                size_t lastMemoryFreed = 0;
                int lastCacheDepthScoured = 0;
            } gc;

#if ET
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
#else // ET
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
#endif // ET
        } status;

        struct LowestDepthManager {
            const int depth;
            ~LowestDepthManager() {
                status.lowestDepthEvaluatedSinceLastGC = min(status.lowestDepthEvaluatedSinceLastGC, depth);
                int lastDepth = depth - 1;
                if (lastDepth < DEPTH_COUNTS_TO_TRACK) {
                    ++status.movesFinishedAtDepth[lastDepth];

                    while (lastDepth > 0 && status.movesFinishedAtDepth[lastDepth] == status.maxMovesAtDepth[lastDepth]) {
                        ++status.movesFinishedAtDepth[lastDepth - 1];
                        status.movesFinishedAtDepth[lastDepth] = -1;
                        status.maxMovesAtDepth[lastDepth] = -1;

                        --lastDepth;
                    }
                }
            }
        };

    public:
        static void run();

        static struct CacheAndLocks {
            template <typename CurMPtr>
            friend class MoveManager;
            friend class RedMoveManager;
            friend class BlackMoveManager;
            friend class SearchTree;
        private:
            std::mutex ioMutex;
            std::mutex gcMutex;
            std::atomic_flag manageMemoryLock;

            MoveCache moveCache[MAX_MOVE_DEPTH];

#if SGC
            Garbage garbage[MAX_MOVE_DEPTH];
#endif // SGC

#if ET | USE_THREADED_GC
            std::mutex moveCacheLocks[MAX_MOVE_DEPTH];
            std::condition_variable moveCVs[MAX_MOVE_DEPTH];
#endif

#if ET
            std::mutex threadFinishedLock;
            std::condition_variable threadFinishedCV;

            std::mutex threadsAvailableLock;
            std::condition_variable threadsAvailableCV;
#endif // ET

#if SGC & ET
            std::mutex garbageLocks[MAX_MOVE_DEPTH];
#endif // USE_SMART_GC & ET
        } cal;
        };
    }