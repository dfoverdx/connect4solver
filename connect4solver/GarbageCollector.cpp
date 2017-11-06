#include <atomic>
#include <cassert>
#include <condition_variable>
#include <sstream>
#include <mutex>
#include <queue>
#include <thread>
#include "GarbageCollector.h"
#include "SearchTree.h"
#include "SearchTree.Macros.h"

using namespace std;
using namespace connect4solver;
using namespace connect4solver::moveCache;

#if VERBOSE
#define addMessageC(stream, color) \
    oss.clear(); \
    oss.str(""); \
    oss << stream << ends; \
    messages.push_back(IoMessage(oss.str().data(), color))

#define addMessage(stream) addMessageC(stream, 0x7)
#else // VERBOSE
#define addMessageC(stream, color) (void(0))
#define addMessage(stream) (void(0))
#endif // VERBOSE

namespace {
#pragma region Prviate function declarations
    ull collectGarbage(const int depth);
    void setMaxMemoryToUse();
    void setMemoryAtInterval();
#pragma endregion

#pragma region Private variables
    bool initialized = false;

    MEMORYSTATUSEX memInfo;
    HANDLE hProcess = GetCurrentProcess();

    atomic_flag manageMemoryLock;
    thread checkMemoryThread;
    mutex gcMutex;

    clock_t lastMemoryCheck;
    ull movesAllowedToBeAddedAfterGarbageCollection = 0;

    queue<BoardHash> hashesToDelete;

    vector<IoMessage> messages;

#if VERBOSE
    ostringstream oss;
#endif // VERBOSE

    gc::GCState state = gc::GCState(&messages);
#pragma endregion
}

#pragma region Extern function definitions
void gc::initGC()
{
    if (initialized) {
        throw exception("Garbage Collector has already been initialized");
    }

    //uLock stateLk(stateMutex);
    if (manageMemoryLock.test_and_set()) {
        throw exception("Somehow gc's manageMemoryLock was already set before initialization.");
    }

    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    setMaxMemoryToUse();
    manageMemoryLock.clear();

    lastMemoryCheck = clock();
    state.messages = &messages;

    checkMemoryThread = thread(setMemoryAtInterval);

    initialized = true;
    //stateLk.unlock();
    //stateCV.notify_all();
}

bool gc::manageMemory(int depth, const ull movesAddedSinceLastGC, const int lowestDepthEvaluatedSinceLastGC, int &lowestDepthEvaluated) {
    return false;
    assert(initialized);

    static auto &pmc = state.pmc;
    static auto &maxMemoryToUse = state.maxMemoryToUse;

    bool ranGC = false;

    et(depth = min(depth, THREAD_AT_DEPTH));
    if (!manageMemoryLock.test_and_set()) {
        lowestDepthEvaluated = min(lowestDepthEvaluated, lowestDepthEvaluatedSinceLastGC);

        if (pmc.PagefileUsage > maxMemoryToUse && depth <= state.cacheFullAt) {
            if (movesAddedSinceLastGC > movesAllowedToBeAddedAfterGarbageCollection) {
                ulGuard(gcMutex);
                //uLock stateLock(stateMutex);

                state.gcStartedAt = clock();
                ull memoryThreshold = maxMemoryToUse;
#if USE_PERCENTAGE_MEMORY
                memoryThreshold = memoryThreshold * ALLOWED_MEMORY_PERCENT / 100;
#else // USE_PERCENTAGE_MEMORY
#if !DBG
                memoryThreshold -= STATIC_MEMORY_REMAINING_LIMIT << 19; // 19 instead of 20 here to be a little lenient
#endif // !DBG
#endif // USE_PERCENTAGE_MEMORY

                messages.clear();

                state.lastGCTime = 0;
                state.minMemoryToClear = pmc.PagefileUsage - memoryThreshold;

                addMessageC("Collecting garbage" << endl, 10);
                addMessage("Delete at least " << (state.minMemoryToClear >> 20) << " MB" << endl);
                //stateUpdated = true;
                //stateLock.unlock();
                //stateCV.notify_all();

                ull garbageCollected = collectGarbage(depth);
                ranGC = true;

                clock_t gcTime = clock() - state.gcStartedAt;

                //stateLock.lock();
                ++state.nextGCRunCount;
                state.lastGCTime = gcTime;
                state.timeSpentGCing += gcTime;
                state.gcStartedAt = 0;

                movesAllowedToBeAddedAfterGarbageCollection = garbageCollected >> 1;

                //setMaxMemoryToUse();
                //stateUpdated = true;
                //stateLock.unlock();
                //stateCV.notify_all();
            }
        }

        manageMemoryLock.clear();
    }

    return ranGC;
}

const gc::GCState& gc::getGCState() {
    return state;
}
#pragma endregion

namespace {
#pragma region Private function definitions
    ull collectGarbage(const int depth) {
        state.runState = gc::GCRunState::running;
        state.lastMemoryFreed = 0;

        bool mightBeOutOfRam = state.lastCacheDepthScoured == 0;

        ull &movesDeleted = state.lastGarbageCollected; // I'm lazy... didn't feel like replacing
        movesDeleted = 0;
        ull totalMovesInTreeBeforeGC = 0;
        usp(ull movesWithMultipleReferences = 0);

        size_t estimatedDataDeleted = 0;
        int startDepth = MAX_MOVE_DEPTH;
        state.lastCacheDepthScoured = 0;

        for (int d = MAX_MOVE_DEPTH - 1; d >= 0; --d) {
            ull cSize = moveCaches[d]->size();
            totalMovesInTreeBeforeGC += cSize;

            if (estimatedDataDeleted < state.minMemoryToClear) {
                startDepth = d;
                if (d % 2 == 0) {
                    estimatedDataDeleted += (cSize * 5 / 10) * (sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData));
                }
                else {
                    estimatedDataDeleted += (cSize * 7 / 10) * (sizeof(pair<BoardHash, MovePtr>) + sizeof(BlackMoveData));
                }
            }
        }

        startDepth = max(startDepth, 14);
        addMessage("Starting at depth " << startDepth << endl);

        for (int e = startDepth; e >= 0; --e) {
            for (int d = e; d < MAX_MOVE_DEPTH; ++d) {
                const bool isRedTurn = d % 2 == 0;
                state.currentGCDepth = d;
                state.lastCacheDepthScoured = e;

                ettgc(lGuard cacheLock(moveCacheMutexes[d]));
                Cache &cache = *moveCaches[d];

                for (auto it : cache) {
                    if (it.second->getRefCount() == 0) {
                        etassert(it.second->getWorkerThread() == 0x7);
                        hashesToDelete.push(it.first);

#if USE_SMART_POINTERS
                        if (it->second.use_count() > 1) {
                            ++movesWithMultipleReferences;
                        }
#endif
                    }
                }

#if VERBOSE
                if (hashesToDelete.size() > 0) {
                    stateLock.lock();
                    addMessage("Erasing " << hashesToDelete.size() << (isRedTurn ? " red" : " black") << " moves at depth=" << d << endl);
                    stateUpdated = true;
                    stateLock.unlock();
                    stateCV.notify_all();
                }
#endif

                movesDeleted += hashesToDelete.size();
                while (!hashesToDelete.empty()) {
                    MovePtr m = cache.at(hashesToDelete.front());
                    assert(m->getRefCount() == 0);

                    if (m->getBlackLost()) {
                        state.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(MoveData);
                        nusp(delete m);
                    }
                    else if (isRedTurn) {
                        assert(d < MAX_MOVE_DEPTH - 1);

                        state.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData);
                        RedMovePtr rptr = rspc(m);

#if ET | TGC
                        decRedNextRefs(rptr, d, cacheLock);
#else // ET | TGC
                        decRedNextRefs(rptr, d);
#endif // ET | TGC

                        nusp(delete rptr);
                    }
                    else {
                        state.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(BlackMoveData);

                        BlackMovePtr bptr = bspc(m);
                        if (d < MAX_MOVE_DEPTH - 1) {
                            BoardHash nextHash = bptr->getNextHash();
                            if (nextHash != 0) {
                                moveCaches[d + 1]->at(nextHash)->decRefCount();
                            }
                        }

                        nusp(delete bptr);
                    }

                    auto erased = cache.erase(hashesToDelete.front());
                    assert(erased == 1);

                    hashesToDelete.pop();
                }

                state.lastPercentMovesDeleted = 100.0 * movesDeleted / totalMovesInTreeBeforeGC;

                if (state.lastMemoryFreed > state.minMemoryToClear && movesDeleted > MIN_GARBAGE_TO_DELETE) {
                    break;
                }
            }

            if (state.lastMemoryFreed > state.minMemoryToClear && movesDeleted > MIN_GARBAGE_TO_DELETE) {
                state.lastCacheDepthScoured = e;
                break;
            }
        }

#if RECREATE_CACHE_AFTER_GC
        addMessage("Remaking caches for depths " << state.lastCacheDepthScoured << " to " << LAST_MOVE_DEPTH);

        for (int d = state.lastCacheDepthScoured; d < MAX_MOVE_DEPTH; ++d) {
#if ET | TGC
            lGuard cacheLock(moveCacheMutexes[d]);
            recreateCache(d, cacheLock);
#else // ET | TGC
            recreateCache(d);
#endif // ET | TGC
        }
#endif // RECREATE_CACHE_AFTER_GC

#if USE_SMART_POINTERS
        if (movesWithMultipleReferences > 0) {
            addMessageC(endl << "Warning: found " << movesWithMultipleReferences << " moves with multiple strong references" << endl, 14);

        }
#endif

        if (state.lastCacheDepthScoured == 0 && mightBeOutOfRam) {
            state.cacheFullAt = depth;
        }
        else {
            state.cacheFullAt = MAX_MOVE_DEPTH;
        }

        state.lastPercentMovesDeleted = 100.0 * movesDeleted / totalMovesInTreeBeforeGC;
        state.totalGarbageCollected += movesDeleted;
        state.runState = gc::GCRunState::stopped;

        return movesDeleted;
#undef addMemoryFreed
    }

    void setMaxMemoryToUse()
    {
        static auto &pmc = state.pmc;
        static auto &maxMemoryToUse = state.maxMemoryToUse;
        GlobalMemoryStatusEx(&memInfo);
        GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));

#if USE_PERCENTAGE_MEMORY
        maxMemoryToUse = max((memInfo.ullAvailPhys + pmc.WorkingSetSize) * ALLOWED_MEMORY_PERCENT / 100, pmc.WorkingSetSize);
#else // USE_PERCENTAGE_MEMORY
#if DBG
        maxMemoryToUse = STATIC_MEMORY_REMAINING_LIMIT << 20;
#else // DBG
        maxMemoryToUse = memInfo.ullAvailPhys + pmc.WorkingSetSize - (STATIC_MEMORY_REMAINING_LIMIT << 20);
#endif // DBG
#endif // USE_PERCENTAGE_MEMORY
    }

    void setMemoryAtInterval() {
        while (true) {
            this_thread::sleep_for(CHECK_MEMORY_INTERAVAL);
            ulGuard(gcMutex);
            int tries = 0;
            constexpr int MaxTries = 100000;
            while ((manageMemoryLock.test_and_set()) && ++tries < MaxTries);
            if (tries >= MaxTries) {
                // let go of the gcMutex to avoid race condition
                continue;
            }

            setMaxMemoryToUse();
            manageMemoryLock.clear();
        }
    }
#pragma endregion
}