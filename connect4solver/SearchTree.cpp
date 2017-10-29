#include <array>
#include <conio.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include "Constants.Board.h"
#include "NextMoveDescriptor.h"
#include "SearchTree.h"
#include "SortingNetwork.h"

//#pragma push_macro("NDEBUG")
//#undef NDEBUG
#include <cassert>
//#pragma pop_macro("NDEBUG")
#include <map>

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace connect4solver;

#define stats status.stats
#define gc status.gc

#pragma region Function declarations
template <class MM>
inline void sortHeuristics(MM &mManager, const bool isSymmetric) {
    static_fail("Should not have accessed generic sortHeuristic()");
};

#if ET

template <typename TData>
inline bool acquireMove(int &outBoardIdx, TDataPtr* &outMove, const uint threadId, const int depth,
    const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
    MoveCache &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies);

template <typename TData>
inline bool shortCircuit(TDataPtr*) {
    throw exception("Somehow tried to call shortCircuit() with something other than a move pointer");
}

template <>
inline bool shortCircuit(BlackMovePtr* ptr) {
    return (*ptr)->getBlackLost();
}

template <>
inline bool shortCircuit(RedMovePtr* ptr) {
    return (*ptr)->getMovesToWin() == 1;
}

void searchThreaded(const int depth, const Board &board, MoveManager &bmm, double progress,
    const double nextProgressChunk, const Board(&nextBoards)[BOARD_WIDTH], MoveCache &cache, const int maxBoards,
    const array<int, BOARD_WIDTH> &moveIndicies);

#endif // ENABLE_THREADING

#pragma endregion

#pragma region Variable declarations
constexpr int PARENT_THREAD_ID = MAX_THREADS - 1;

clock_t lastMemoryCheck;
ull movesAllowedToBeAddedAfterGarbageCollection = 0;
MEMORYSTATUSEX memInfo;
HANDLE hProcess = GetCurrentProcess();

atomic<bool> checkMemory(false);
thread checkMemoryThread;

thread printStatusThread;

#if ET
Board allBoards[MAX_THREADS][MAX_MOVE_DEPTH][BOARD_WIDTH];
atomic<bool> threadDepthFinished[MAX_MOVE_DEPTH];
#else // ET
Board boards[MAX_MOVE_DEPTH][BOARD_WIDTH];
#endif // ET

#if !USE_SMART_GC
#ifdef NDEBUG
queue<BoardHash> hashesToDelete;
#else // NDEBUG
queue<BoardHash> hashesToDelete;
#endif // NDEBUG
#endif // !USE_SMART_GC

#pragma endregion

#pragma region SearchTree member definitions

SearchTree::SearchTreeStatus SearchTree::status;

SearchTree::CacheAndLocks SearchTree::cal;
void SearchTree::run()
{
#if ET
    // set all threads to available
    threadsAvailable.flip();

    for (int i = 0; i < MAX_MOVE_DEPTH; ++i) {
        threadDepthFinished[i] = false;
    }

    for (int i = 0; i < MAX_THREADS; ++i) {
        threadProgress[i] = 0.0;
    }

    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < MAX_THREADS; ++j) {
            allCurrentMoveRoutes[j][i] = -1;
        }
    }
#else
    for (int i = 0; i < BOARD_SIZE; ++i) {
        status.currentMoveRoute[i] = -1;
    }
#endif

    assert(!cal.manageMemoryLock.test_and_set());
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    setMaxMemoryToUse();
    cal.manageMemoryLock.clear();

    printStatusThread = thread(printStatus);
    checkMemoryThread = thread(setCheckMemoryAtInterval);

    Board b = Board().addPiece(3);

    int maxDepthRequired = 0;
    RedMovePtr dummy = RedMovePtr(new RedMoveData(true));

#if ET
    if (!dummy->acquire(0)) {
        throw exception("could not acquire first move.....?");
    }
#endif

    lastMemoryCheck = clock();
    search<RedMovePtr>(0, PARENT_THREAD_ID, b, 0, 1.0, dummy);
}

// enumerate board hashes
// check if they've already been calculated
// if not, generate the board and add it to our MoveManager
template<typename CurMPtr, class MM>
inline bool SearchTree::enumerateBoards(const int depth, const int maxBoards, const Board &board, MM &mManager,
    Board(&nextBoards)[BOARD_WIDTH], int &invalidMoves, int &finishedMoves) {
    typedef conditional<is_same<CurMPtr, BlackMovePtr>::value, RedMovePtr, BlackMovePtr>::type NextMPtr;

    const int nextDepth = depth + 1;
    MoveCache &nextCache = cal.moveCache[nextDepth];

    const Heuristic DONT_RECALCULATE_HEURISTIC = (depth % 2 == 0) ? BLACK_WON_HEURISTIC : CATS_GAME_HEURISTIC;
    bool canEndOnCompletion = false;

    ettgc(lGuard(cal.moveCacheLocks[nextDepth]));
    sgc(ettgc(lGuard(cal.garbageLocks[nextDepth])));
    for (int i = 0; i < maxBoards; ++i) {
        if (!board.isValidMove(i)) {
            ++invalidMoves;
            mManager[i] = { i, 0, DONT_RECALCULATE_HEURISTIC, nullptr };
            continue;
        }

        BoardHash nextHash = board.getNextBoardHash(i);
        mManager[i].col = i;
        mManager[i].hash = nextHash;

        auto it = nextCache.find(nextHash);
        if (it != nextCache.end()) {
            ++stats.cacheHits;

#if USE_SMART_GC
            if (it->second->incRefCount() == 1) {
                bool removed = cal.garbage[nextDepth].erase(nextHash) != 0;
                assert(removed);
            }
#else // USE_SMART_GC
            it->second->incRefCount();
#endif // USE_SMART_GC

            mManager[i].move = rpc(NextMPtr*, &it->second);

            if (it->second->getIsFinished()) {
                ++finishedMoves;
                mManager[i].heur = DONT_RECALCULATE_HEURISTIC;

                switch (doMoveEvaluation(mManager, i)) {
                case continueEval::finished:
                    return true;

                case continueEval::finishedAfterCheckingNewBoards:
                    // still have to finish enumerating new boards if there are any
                    canEndOnCompletion = true;
                    continue;

                case continueEval::unfinishedEval:
                    continue;
                }
            }
        }
        else {
            ++stats.cacheMisses;
        }

        if (mManager[i].heur == 0) {
            Board &nextBoard = nextBoards[i] = board.addPiece(i);
            assert(nextBoard.boardHash == nextHash);

            if (mManager[i].move == nullptr) {
                ++status.movesAddedSinceLastGC;
            }

            mManager.addMove(i, nextBoard);
            if (doHeuristicEvaluation(mManager, nextBoard, i, finishedMoves)) {
                return true;
            }
        }
    }

    return canEndOnCompletion;
}

template <>
inline static SearchTree::continueEval SearchTree::doMoveEvaluation<BlackMoveManager>(BlackMoveManager &mManager, const int i) {
    RedMovePtr* mv = mManager[i].move;
    assert((*mv)->getIsFinished());

    if ((*mv)->getBlackLost()) {
        return continueEval::unfinishedEval;
    }

    int mtw = (*mv)->getMovesToWin() + 1;
    if (mtw < mManager.movesToWin) {
        mManager.bestMove = mManager[i].col;
        mManager.movesToWin = mtw;
    }

    return mtw == 2 ? continueEval::finishedAfterCheckingNewBoards : continueEval::unfinishedEval;
}

template <>
inline static SearchTree::continueEval SearchTree::doMoveEvaluation<RedMoveManager>(RedMoveManager &mManager, const int i) {
    BlackMovePtr* mv = mManager[i].move;
    assert((*mv)->getIsFinished());

    if ((*mv)->getBlackLost()) {
        mManager.movesToWin = BLACK_LOST;
        return continueEval::finished;
    }

    int mtw = (*mv)->getMovesToWin() + 1;
    if (mtw > mManager.movesToWin) {
        mManager.movesToWin = mtw;
    }

    return continueEval::unfinishedEval;
}

template<>
inline static bool SearchTree::doHeuristicEvaluation<BlackMoveManager>(BlackMoveManager &mManager, const Board& board, const int i, int &finishedMoves) {
    switch (board.getGameOver()) {
    case gameOverState::blackLost:
        mManager[i].heur = board.heuristic;
        ++finishedMoves;
        return false;

    case gameOverState::unfinished:
        mManager[i].heur = board.heuristic;
        return false;

    case gameOverState::blackWon:
        mManager.bestMove = i;
        mManager.movesToWin = 0;
        return true;

    default:
        throw exception("Somehow returned an invalid gameOverState");
    }
}

template<>
inline static bool SearchTree::doHeuristicEvaluation<RedMoveManager>(RedMoveManager &mManager, const Board& board, const int i, int &finishedMoves) {
    switch (board.getGameOver()) {
    case gameOverState::blackWon:
        mManager[i].heur = board.heuristic;
        ++finishedMoves;
        return false;

    case gameOverState::unfinished:
        mManager[i].heur = board.heuristic;
        return false;

    case gameOverState::blackLost:
        mManager.movesToWin = BLACK_LOST;
        return true;

    default:
        throw exception("Somehow returned an invalid gameOverState");
    }
}

template <>
inline bool SearchTree::doSearchLast<RedMovePtr>(const int depth, const Board &board, RedMovePtr &curMovePtr) {
    assert(depth < LAST_MOVE_DEPTH);
    return false;
}

template <>
inline bool SearchTree::doSearchLast<BlackMovePtr>(const int depth, const Board &board, BlackMovePtr &curMovePtr) {
    assert(depth < MAX_MOVE_DEPTH);
    if (depth == LAST_MOVE_DEPTH) {
        searchLast(board, curMovePtr);
        return true;
    }

    return false;
}

// returns moves til win:
//   true: move finished
//   false: move processing was interrupted by parent thread short-circuiting
template<typename CurMPtr>
inline bool SearchTree::search(const int depth, const uint threadId, const Board &board, double progress, const double progressChunk, CurMPtr &curMovePtr)
{
    constexpr bool isRedTurn = is_same<CurMPtr, RedMovePtr>::value;
    const int nextDepth = depth + 1;

    if (isRedTurn) {
        assert(depth % 2 == 0);
    }
    else {
        assert(depth % 2 == 1);
    }

    ++status.movesEvaluated;

    if (doSearchLast(depth, board, curMovePtr)) {
        return true;
    }

    manageMemory(depth);

    typedef conditional<isRedTurn, BlackMovePtr, RedMovePtr>::type NextMPtr;
    typedef conditional<isRedTurn, RedMoveManager, BlackMoveManager>::type MM;
    assert(curMovePtr != nullptr);
    assert(!curMovePtr->getIsFinished());
    etassert(curMovePtr->getWorkerThread() == threadId);

    LowestDepthManager ldm{ depth };
    const int maxBoards = board.symmetryBF.isSymmetric ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
    Board(&nextBoards)[BOARD_WIDTH] = boards[nextDepth];
    MoveCache &nextCache = cal.moveCache[nextDepth];

    if (board.symmetryBF.isSymmetric) {
        ++stats.symmetricBoards;
    }
    else if (board.symmetryBF.possiblySymmetric) {
        ++stats.possiblySymmetricBoards;
    }
    else {
        ++stats.asymmetricBoards;
    }

    setProgress(progress);

    int finishedMoves = 0;
    int invalidMoves = 0;
    MM mManager(depth, board, &curMovePtr);
    double tmpOld;
    double tmpNew;

    if (enumerateBoards<CurMPtr, MM>(depth, maxBoards, board, mManager, nextBoards, invalidMoves, finishedMoves)) {
        if (isRedTurn) {
            assert(mManager.movesToWin == BLACK_LOST);
        }
        else {
            assert(mManager.movesToWin == 0 || mManager.movesToWin == 2);
        }

        do {
            tmpOld = status.progressSaved;
            tmpNew = tmpOld + progressChunk;
        } while (!status.progressSaved.compare_exchange_strong(tmpOld, tmpNew));

        return true;
    }

    const int newBoards = maxBoards - finishedMoves - invalidMoves;

    if (depth < DEPTH_COUNTS_TO_TRACK) {
        status.maxMovesAtDepth[depth] = newBoards;
        status.movesFinishedAtDepth[depth] = 0;

        //for (int i = depth + 1; i < DEPTH_COUNTS_TO_TRACK; ++i) {
        //    status.maxMovesAtDepth[i] = -1;
        //    status.movesFinishedAtDepth[i] = -1;
        //}
    }

    do {
        tmpOld = status.progressSaved;
        tmpNew = tmpOld + finishedMoves / double(maxBoards - invalidMoves) * progressChunk;
    } while (!status.progressSaved.compare_exchange_strong(tmpOld, tmpNew));

    const double nextProgressChunk = progressChunk / newBoards;

    if (finishedMoves + invalidMoves == maxBoards) { // all moves have been previously evaluated
        return true;
    }

    sortHeuristics(mManager, board.symmetryBF.isSymmetric);

    // TODO: change to using a MoveManager::iterator
    for (int i = 0; i < newBoards; ++i) {
        int nextBoardIdx = mManager[i].col;
        assert(board.validMoves.cols[nextBoardIdx]);
        NextMPtr* nextMovePtr = mManager[i].move;
        assert(nextMovePtr != nullptr && *nextMovePtr != nullptr);
        assert(!(*nextMovePtr)->getIsFinished()); // may need to change this for threading--hard to say

        const Board &nextBoard = nextBoards[nextBoardIdx];

        status.currentMoveRoute[depth] = nextBoardIdx;

        bool finished = search<NextMPtr>(nextDepth, threadId, nextBoard, progress, nextProgressChunk, *nextMovePtr);

        status.currentMoveRoute[depth] = -1;

#if ET
        if (!finished) {
            // thread aborted
            curMovePtr->releaseWithoutFinish();
            return -1;
        }
#else
        assert(finished);
#endif

        switch (doMoveEvaluation(mManager, i)) { // intentionally use i here rather than nextBoardIdx
        case continueEval::finished:
        case continueEval::finishedAfterCheckingNewBoards:
            if (isRedTurn) {
                assert(mManager.movesToWin == BLACK_LOST);
            }
            else {
                assert(mManager.movesToWin == 0 || mManager.movesToWin == 2);
            }

            return true;
        }

        progress += nextProgressChunk;
        setProgress(progress);
    }

    return true;
}

// searches the last move
inline bool SearchTree::searchLast(const Board &board, BlackMovePtr &curMovePtr) {
    assert(curMovePtr != nullptr);
    assert(!curMovePtr->getIsFinished());
    etassert(curMovePtr->getWorkerThread() == threadId);
    constexpr int DEPTH = LAST_MOVE_DEPTH;

    const int maxBoards = board.symmetryBF.isSymmetric ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
    if (board.symmetryBF.isSymmetric) {
        ++stats.symmetricBoards;
    }
    else if (board.symmetryBF.possiblySymmetric) {
        ++stats.possiblySymmetricBoards;
    }
    else {
        ++stats.asymmetricBoards;
    }

    // if this is the max depth, check all next possible boards
    // if winning board found, finish this move and return
    for (int i = 0; i < maxBoards; ++i) {
        if (!board.validMoves.cols[i]) {
            continue;
        }

        Board lastBoard = board.addPiece(i);
        if (lastBoard.getGameOver() == gameOverState::blackWon) {
            curMovePtr->setNextHash(lastBoard.boardHash);
            curMovePtr->finish(0);
            return true;
        }
    }

    MoveData::setBlackLost(rpc(MovePtr*, &curMovePtr));
    return true;
}

void SearchTree::decRedNextRefs(RedMovePtr &ptr, const int depth) {
    etassert(cal.moveCacheLocks[depth + 1].owns_lock());
    sgc(etassert(cal.garbageLocks[depth + 1].owns_lock()));
    int size;
    const BoardHash* hashes = ptr->getNextHashes(size);
    MoveCache &cache = cal.moveCache[depth + 1];

#if SGC
    Garbage &garbage = cal.garbage[depth + 1];
    for (int i = 0; i < size; ++i) {
        if (hashes[i] != 0) {
            if (cache.at(hashes[i])->decRefCount() == 0) {
                auto added = garbage.insert(hashes[i]);
                assert(added.second);
            }
        }
    }
#else // SGC
    for (int i = 0; i < size; ++i) {
        if (hashes[i] != 0) {
            cache.at(hashes[i])->decRefCount();
        }
    }
#endif // SGC
}

#if USE_SMART_GC
ull SearchTree::garbageCollect() {
    ull movesDeleted = 0;
    ull totalMovesInTreeBeforeGC = 0;
    usp(ull movesWithMultipleReferences = 0);
    usp(static_fail("TODO: add multiple references to gc struct in SearchTree"));

    gc.lastMemoryFreed = 0;
#define addMemoryFreed(val) gc.lastMemoryFreed += val

    for (int d = 0; d < MAX_MOVE_DEPTH; ++d) {
        const bool isRedTurn = d % 2 == 0;
        MoveCache &cache = cal.moveCache[d];
        totalMovesInTreeBeforeGC += cache.size();

        for (auto &hit : cal.garbage[d]) {
            MovePtr m = cache.at(hit);
            assert(m->getRefCount() == 0);

            if (m->getBlackLost()) {
                addMemoryFreed(sizeof(pair<BoardHash, MovePtr>) + sizeof(MoveData));
                nusp(delete m);
            }
            else if (isRedTurn) {
                assert(d < MAX_MOVE_DEPTH - 1);

#if USE_DYNAMIC_SIZE_RED_NEXT
                addMemoryFreed(sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData) + (m->getIsSymmetric() ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH));
#else
                addMemoryFreed(sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData));
#endif
                RedMovePtr rptr = rspc(m);
                decRedNextRefs(rptr, d);
                nusp(delete rptr);
            }
            else {
                addMemoryFreed(sizeof(pair<BoardHash, MovePtr>) + sizeof(BlackMoveData));

                BlackMovePtr bptr = bspc(m);
                if (d < MAX_MOVE_DEPTH - 1) {
                    BoardHash nextHash = bptr->getNextHash();
                    if (nextHash != 0) {
                        MovePtr next = cal.moveCache[d + 1].at(nextHash);
                        if (next->decRefCount() == 0) {
                            auto added = cal.garbage[d + 1].insert(nextHash);
                            assert(added.second);
                        }
                    }
                }

                nusp(delete bptr);
            }

            auto erased = cache.erase(hit);
            assert(erased == 1);
        }

        addMemoryFreed(sizeof(BoardHash) * cal.garbage[d].size());
        movesDeleted += cal.garbage[d].size();
        cal.garbage[d] = Garbage();
    }

    gc.lastPercentMovesDeleted = 100.0 * movesDeleted / totalMovesInTreeBeforeGC;
    gc.lastGarbageCollected = movesDeleted;
    gc.totalGarbageCollected += movesDeleted;
    return movesDeleted;
#undef addMemoryFreed
}
#else // USE_SMART_GC
ull SearchTree::garbageCollect(const int depth, size_t minMemoryToClear)
{
    gc.lastMemoryFreed = 0;
    bool mightBeOutOfRam = gc.lastCacheDepthScoured == 0;

    ull movesDeleted = 0;
    ull totalMovesInTreeBeforeGC = 0;
#if USE_SMART_POINTERS
    ull movesWithMultipleReferences = 0;
#endif

    size_t estimatedDataDeleted = 0;
    int startDepth = MAX_MOVE_DEPTH;
    gc.lastCacheDepthScoured = 0;

    for (int d = MAX_MOVE_DEPTH - 1; d >= 0; --d) {
        ull cSize = cal.moveCache[d].size();
        totalMovesInTreeBeforeGC += cSize;

        if (estimatedDataDeleted < minMemoryToClear) {
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
    cout << "Starting at depth " << startDepth << endl;

    for (int e = startDepth; e >= 0; --e) {
        for (int d = e; d < MAX_MOVE_DEPTH; ++d) {
            const bool isRedTurn = d % 2 == 0;

            ettgc(lGuard(moveCacheMutexes[d]));
            MoveCache &cache = cal.moveCache[d];

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
                cout << "Erasing " << hashesToDelete.size() << (isRedTurn ? " red" : " black") << " moves at depth=" << d << '\n';
            }
#endif

            movesDeleted += hashesToDelete.size();
            while (!hashesToDelete.empty()) {
                MovePtr m = cache.at(hashesToDelete.front());
                assert(m->getRefCount() == 0);

                if (m->getBlackLost()) {
                    gc.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(MoveData);
                    nusp(delete m);
                }
                else if (isRedTurn) {
                    assert(d < MAX_MOVE_DEPTH - 1);

#if USE_DYNAMIC_SIZE_RED_NEXT
                    gc.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData) + (m->getIsSymmetric() ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH);
#else
                    gc.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(RedMoveData);
#endif
                    RedMovePtr rptr = rspc(m);
                    decRedNextRefs(rptr, d);
                    nusp(delete rptr);
                }
                else {
                    gc.lastMemoryFreed += sizeof(pair<BoardHash, MovePtr>) + sizeof(BlackMoveData);

                    BlackMovePtr bptr = bspc(m);
                    if (d < MAX_MOVE_DEPTH - 1) {
                        BoardHash nextHash = bptr->getNextHash();
                        if (nextHash != 0) {
                            cal.moveCache[d + 1].at(nextHash)->decRefCount();
                        }
                    }

                    nusp(delete bptr);
                }

                auto erased = cache.erase(hashesToDelete.front());
                assert(erased == 1);

                hashesToDelete.pop();
            }

            if (gc.lastMemoryFreed > minMemoryToClear && movesDeleted > MIN_GARBAGE_TO_DELETE) {
                break;
            }
        }

        if (gc.lastMemoryFreed > minMemoryToClear && movesDeleted > MIN_GARBAGE_TO_DELETE) {
            gc.lastCacheDepthScoured = e;
            break;
        }
    }

#if USE_SMART_POINTERS
    static_fail("")
    if (movesWithMultipleReferences > 0) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
        cout << "\nWarning: found " << movesWithMultipleReferences << " moves with multiple strong references\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }
#endif

    if (gc.lastCacheDepthScoured == 0 && mightBeOutOfRam) {
        gc.cacheFullAt = depth;
    }
    else {
        gc.cacheFullAt = MAX_MOVE_DEPTH;
    }

    gc.lastPercentMovesDeleted = 100.0 * movesDeleted / totalMovesInTreeBeforeGC;
    gc.lastGarbageCollected = movesDeleted;
    gc.totalGarbageCollected += movesDeleted;
    return movesDeleted;
#undef addMemoryFreed
}
#endif

void SearchTree::manageMemory(int depth) {
    static auto &pmc = status.pmc;
    static auto &maxMemoryToUse = status.maxMemoryToUse;

    et(depth = min(depth, THREAD_AT_DEPTH));
    if (!cal.manageMemoryLock.test_and_set()) {
        status.lowestDepthEvaluated = min(status.lowestDepthEvaluated, status.lowestDepthEvaluatedSinceLastGC);

        if (checkMemory) {
            GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
            if (pmc.PagefileUsage > maxMemoryToUse && depth <= gc.cacheFullAt) {
                if (status.movesAddedSinceLastGC > movesAllowedToBeAddedAfterGarbageCollection) {
                    lGuard(cal.ioMutex);
                    lGuard(cal.gcMutex);

                    clock_t gcTime = clock();
                    ull memoryThreshold = maxMemoryToUse;
#if USE_PERCENTAGE_MEMORY
                    memoryThreshold = memoryThreshold * ALLOWED_MEMORY_PERCENT / 100;
#else // USE_PERCENTAGE_MEMORY
#ifdef NDEBUG
                    memoryThreshold -= STATIC_MEMORY_REMAINING_LIMIT << 19; // 19 instead of 20 here to be a little lenient
#endif // NDEBUG
#endif // USE_PERCENTAGE_MEMORY

                    ull minMemoryToClear = pmc.PagefileUsage - memoryThreshold;

                    cout << setColor(10) << "Collecting garbage" << endl << resetColor;
                    cout << "Delete at least " << (minMemoryToClear >> 20) << " MB" << endl;
                    ull garbageCollected = garbageCollect(depth, minMemoryToClear);
                    clearScreen();

                    gcTime = clock() - gcTime;
                    ++gc.nextGCCount;
                    gc.lastGCTime = gcTime;
                    gc.timeSpentGCing += gcTime;

                    movesAllowedToBeAddedAfterGarbageCollection = garbageCollected >> 1;
                    status.movesAddedSinceLastGC = 0;

                    setMaxMemoryToUse();

                    status.lowestDepthEvaluatedSinceLastGC = MAX_MOVE_DEPTH;
                }
            }

            checkMemory = false;
        }

        cal.manageMemoryLock.clear();
    }

}

void SearchTree::setCheckMemoryAtInterval() {
    while (true) {
        this_thread::sleep_for(CHECK_MEMORY_INTERAVAL);
        lGuard(cal.gcMutex);
        int tries = 0;
        constexpr int maxTries = 100000;
        while ((cal.manageMemoryLock.test_and_set()) && ++tries < maxTries);
        if (tries >= maxTries) {
            // let go of the gcMutex to avoid race condition
            continue;
        }

        checkMemory = true;
        cal.manageMemoryLock.clear();
    }
}

void SearchTree::setMaxMemoryToUse() {
    static auto &pmc = status.pmc;
    static auto &maxMemoryToUse = status.maxMemoryToUse;
    GlobalMemoryStatusEx(&memInfo);
    GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));

#if USE_PERCENTAGE_MEMORY
    maxMemoryToUse = max((memInfo.ullAvailPhys + pmc.WorkingSetSize) * ALLOWED_MEMORY_PERCENT / 100, pmc.WorkingSetSize);
#else // USE_PERCENTAGE_MEMORY
#ifdef NDEBUG
    maxMemoryToUse = memInfo.ullAvailPhys + pmc.WorkingSetSize - (STATIC_MEMORY_REMAINING_LIMIT << 20);
#else // NDEBUG
    maxMemoryToUse = STATIC_MEMORY_LIMIT << 20;
#endif // NDEBG
#endif // USE_PERCENTAGE_MEMORY
}

#pragma endregion

#pragma region Function definitions

template <>
inline void sortHeuristics<BlackMoveManager>(BlackMoveManager &mManager, const bool isSymmetric) {
    if (isSymmetric) {
        SortingNetwork::Sort4Reverse(mManager.moves);
    }
    else {
        SortingNetwork::Sort7Reverse(mManager.moves);
    }
}

template <>
inline void sortHeuristics<RedMoveManager>(RedMoveManager &mManager, const bool isSymmetric) {
    if (isSymmetric) {
        SortingNetwork::Sort4(mManager.moves);
    }
    else {
        SortingNetwork::Sort7(mManager.moves);
    }
}

inline void printParentThreadShortCircuited(int threadId) {
#if VERBOSE
    static_fail(TODO_VERBOSE);
    lGuard(ioMutex);

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
    cout << "Parent node has been evaluated.  Thread " << threadId << " can return without finishing.\n";
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#endif
}

#if ET
static_fail("TODO: Remove this and move it to MoveManager::iterator");
// returns true if a moves was acquired, else false (either becauses all moves are finished or the thread was interrupted
// waits if all next moves are currently locked but at least one is unfinished
template <typename TData>
inline bool acquireMove(int &outBoardIdx, TDataPtr* &outMove, const uint threadId, const int depth,
    const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
    MoveCache &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies) {

    outMove = nullptr;
    outBoardIdx = -1;

    if (threadDepthFinished[parentThreadDepth]) {
        printParentThreadShortCircuited(threadId);
        return false;
    }

    const int nextDepth = depth + 1;

    int movesFinished = 0;

    // check for unfinished, unlocked moves
    {
        ettgc(lGuard(moveCacheMutexes[nextDepth]));
        for (int i = 0; i < maxBoards; ++i) {
            int x = moveIndicies[i];
            if (!curBoard.isValidMove(x)) {
                // we know that all the invalid moves were shoved to the back of the array
                break;
            }

            MovePtr* tmpMvPtr = &cache.at(nextBoards[x].boardHash);
            if (shortCircuit(rpc(TDataPtr*, tmpMvPtr))) {
                outMove = rpc(TDataPtr*, tmpMvPtr);
                outBoardIdx = i;
                return false;
            }

            if ((*tmpMvPtr)->getIsFinished()) {
                ++movesFinished;
                continue;
            }

            bool acquiredMove = (*tmpMvPtr)->acquire(threadId);
            if (acquiredMove) {
                outBoardIdx = x;
                outMove = rpc(TDataPtr*, tmpMvPtr);
                return true;
            }
        }
    }

    if (movesFinished == maxBoards) {
        outBoardIdx = 0;
        return false;
    }

    // all unfinished moves are locked
    for (int i = 0; i < maxBoards; ++i) {
        if (!curBoard.isValidMove(i)) {
            // we know that all the invalid moves were shoved to the back of the array
            break;
        }

        BoardHash nHash = nextBoards[i].boardHash;

        duration<uint> timeWaited = 0s;
        auto predicate = [cache, depth, nHash, threadId] {
            const MovePtr* tmpMvPtr = &cache.at(nHash);

            return (*tmpMvPtr)->getIsFinished() || (*tmpMvPtr)->acquire(threadId);
        };

        uLock cacheLk(moveCacheMutexes[nextDepth]);
        while (!moveCVs[depth].wait_for(cacheLk, TIME_TO_WAIT_FOR_CVS, predicate)) {
            timeWaited += duration_cast<seconds>(TIME_TO_WAIT_FOR_CVS);
            uLock ioLk(ioMutex);
            cout << "Thread " << threadId <<
                " at depth " << depth <<
                " has waited " << timeWaited.count() <<
                " seconds for a move owned by thread " <<
                cache.at(nHash)->getWorkerThread() <<
                " to finish.\n";
            ioLk.unlock();

            if (threadDepthFinished[parentThreadDepth]) {
                printParentThreadShortCircuited(threadId);
                cacheLk.unlock();
                return false;
            }
        }

        cacheLk.unlock();

        MovePtr* tmpMvPtr = &cache.at(nHash);
        if ((*tmpMvPtr)->getWorkerThread() == threadId) {
            assert(!(*tmpMvPtr)->getIsFinished());
            outMove = rpc(TDataPtr*, tmpMvPtr);
            outBoardIdx = i;
            return true;
        }

        if (shortCircuit(rpc(TDataPtr*, tmpMvPtr))) {
            outMove = rpc(TDataPtr*, tmpMvPtr);
            outBoardIdx = i;
            return false;
        }
    }

    return false;
}
#endif

#if ET
static_fail("TODO: Remove this and move it to MoveManager::iterator");
const bool acquireThreadId(int &childThreadId) {
    et(lGuard(threadsAvailableLock));
    if (threadsAvailable.none()) {
        childThreadId = -1;
        return false;
    }

    for (int i = 0; i < MAX_THREADS; ++i) {
        if (threadsAvailable[i]) {
            threadsAvailable[i] = false;
            childThreadId = i;
            return true;
        }
    }

    throw exception("Somehow didn't find an available thread when threadsAvailable wasn't none()");
}

// wrapper for running the searches and then notifying when finished
int doWork(int nextDepth, int childThreadId, const int boardMove, int depth, const Board& nextBoard, const double nextProgressChunk, RedMovePtr &nextMovePtr) {
#if VERBOSE
    static_fail(TODO_VERBOSE);
    {
        lGuard(ioMutex);
        cout << "Thread " << childThreadId << " working on move " << boardMove << "\n";
    }
#endif

    try {
        int val = searchRed(nextDepth, childThreadId, depth, nextBoard, 0.0, nextProgressChunk, nextMovePtr);

#if VERBOSE

        {
            lGuard(ioMutex);
            cout << "Thread " << childThreadId << " finished working on move " << boardMove << ": " << val << "\n";
        }
#endif

        uLock lk(threadFinishedMutex);
        threadFinishedCV.wait(lk, [] { return true; });
        threadFinished[childThreadId] = true;
        lk.unlock();

#if VERBOSE
        {
            lGuard(ioMutex);
            cout << "Thread " << childThreadId << " set thread finished\n";
        }
#endif

        threadFinishedCV.notify_one();

        return val;
    }
    catch (exception &e) {
        lGuard(ioMutex);
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
        cout << "Thread " << childThreadId << " failed with exception:\n" << e.what() << "\n\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);

        return -2;
    }
}

void searchThreaded(const int depth, const Board &board, MoveManager &bmm, double progress,
    const double nextProgressChunk, const Board(&nextBoards)[BOARD_WIDTH], MoveCache &cache, const int maxBoards,
    const array<int, BOARD_WIDTH> &moveIndicies)
{
    assert(depth == THREAD_AT_DEPTH);

#if VERBOSE
    static_fail(TODO_VERBOSE);
    {
        lGuard(ioMutex);
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 11);
        cout << "Splitting\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }
#endif

    const int threadId = -1;
    const int nextDepth = depth + 1;
    const int parentThreadDepth = -1;
    future<int> movesToWin[MAX_THREADS];

    Board threadBoards[MAX_THREADS];
    int threadBoardIdxs[MAX_THREADS];
    RedMovePtr* nextMovePtrs[MAX_THREADS];

    static_fail("There is *no way* this is the simplest/cleanest way to do this");
    while (true) {
        int childThreadId = -1;
        while (acquireThreadId(childThreadId)) {
            assert(childThreadId / MAX_THREADS == 0); // thread id is valid

            Board &tBoard = threadBoards[childThreadId];
            int &tBoardIdx = threadBoardIdxs[childThreadId];
            RedMovePtr* &nextMovePtr = nextMovePtrs[childThreadId];
            future<int> &mtw = movesToWin[childThreadId];
            if (acquireMove(tBoardIdx, nextMovePtr, childThreadId, depth, -1, board, nextBoards, cache, maxBoards,
                moveIndicies)) {
                assert(nextMovePtr != nullptr && *nextMovePtr != nullptr);
                assert((*nextMovePtr)->getWorkerThread() == childThreadId);

                tBoard = nextBoards[tBoardIdx];
                allCurrentMoveRoutes[childThreadId][depth] = tBoardIdx;

                mtw = async(std::launch::async, &doWork, nextDepth, childThreadId, tBoardIdx, depth, tBoard,
                    nextProgressChunk, ref(*nextMovePtr));
            }
            else {
                et(lGuard(threadsAvailableMutex));
                threadsAvailable[childThreadId] = true;
                break;
            }
        }

#if VERBOSE

        {
            lGuard(ioMutex);
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
            cout << "Spawned " << MAX_THREADS - threadsAvailable.count() << " thread(s)\n\n";
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
        }
#endif

        {
            et(lGuard(threadsAvailableMutex));
            if (threadsAvailable.all()) {
                // all values are finished being calculated
                // exit loop
                break;
            }
        }

        uLock lk(threadFinishedMutex);
        threadFinishedCV.wait(lk, [] {
            return threadFinished.any();
        });

        lk.unlock();
        threadFinishedCV.notify_one();

        while (threadFinished.any()) {
            for (uint cThreadId = 0; cThreadId < MAX_THREADS; ++cThreadId) {
                if (!threadFinished[cThreadId]) {
                    continue;
                }

#if VERBOSE

                {
                    lGuard(ioMutex);
                    cout << "Evaluating result from thread " << cThreadId << "\n";
                }
#endif

                lk.lock();
                threadFinishedCV.wait(lk, [] { return true; });
                threadFinished[cThreadId] = false;
                threadsAvailable[cThreadId] = true;
                lk.unlock();

                const Board tBoard = threadBoards[cThreadId];
                const int tBoardIdx = threadBoardIdxs[cThreadId];
                const RedMovePtr* nextMovePtr = nextMovePtrs[cThreadId];

                int tmpMovesTilWin = movesToWin[cThreadId].get();
                allCurrentMoveRoutes[cThreadId][depth] = -1;

                // thread aborted shouldn't occur here, since we don't tell the threads they're aborted until later
                assert(tmpMovesTilWin != -1);

                if (tmpMovesTilWin != BLACK_LOST) {
                    ++tmpMovesTilWin;
                    if (tmpMovesTilWin < bmm.movesToWin) {
                        bmm.movesToWin = tmpMovesTilWin;
                        bmm.bestHash = tBoard.boardHash;
                        bmm.bestMove = tBoardIdx;
                    }

                    // short-circuit if black can force a win with its next move
                    if (bmm.movesToWin == 2) {
                        threadDepthFinished[depth] = true;

                        // finishing of current move is done in the calling searchBlack() function
                        // so is garbage collection

                        for (int i = 0; i < MAX_THREADS; ++i) {
                            // wait for all threads to return
                            int tmp = movesToWin[i].get();
                            if (i != cThreadId && nextMovePtrs[i] != nullptr) {
                                // if the thread finished before being short-circuited, tmp should not be -1
                                // if the thread was short-circuited, tmp should be -1
                                assert((tmp == -1) ^ (*nextMovePtrs[i])->getIsFinished());
                            }
                        }

                        threadDepthFinished[depth] = false;
                        return;
                    }
                }
                else {
                    assert((*nextMovePtr)->getBlackLost());
                }

                progress += nextProgressChunk;
                threadProgress[MAX_THREADS] = progress;
            }
        }
    }

#if VERBOSE

    {
        lGuard(ioMutex);
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 11);
        cout << "Threads joined\n\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }
#endif
}

#endif
#pragma endregion