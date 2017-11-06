#include <array>
#include <string>
#include <type_traits>
#include "Constants.Board.h"
#include "GarbageCollector.h"
#include "NextMoveDescriptor.h"
#include "ProgressPrinter.h"
#include "SearchTree.h"

//#pragma push_macro("NDEBUG")
//#undef NDEBUG
#include <cassert>
//#pragma pop_macro("NDEBUG")
#include <map>

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace connect4solver;
using namespace connect4solver::moveCache;

#define stats state.stats

#pragma region Function declarations
namespace {
    SearchTree::SearchTreeState state;

    enum continueEval {
        unfinishedEval,
        finished,
        finishedAfterCheckingNewBoards
    };

    struct LowestDepthManager {
        const int depth;
        ~LowestDepthManager() {
            state.lowestDepthEvaluatedSinceLastGC = min(state.lowestDepthEvaluatedSinceLastGC, depth);
            int lastDepth = depth - 1;
            if (lastDepth < DEPTH_COUNTS_TO_TRACK) {
                ++state.movesFinishedAtDepth[lastDepth];

                while (lastDepth > 0 && state.movesFinishedAtDepth[lastDepth] == state.maxMovesAtDepth[lastDepth]) {
                    ++state.movesFinishedAtDepth[lastDepth - 1];
                    state.movesFinishedAtDepth[lastDepth] = -1;
                    state.maxMovesAtDepth[lastDepth] = -1;

                    --lastDepth;
                }
            }
        }
    };

    template<typename CurMPtr>
    bool search(const int depth, const uint threadId, const Board &board, double progress, const double progressChunk,
        CurMPtr curMovePtr);
    
    static bool searchLast(const Board &board, const MovePtr curMovePtr);

    template<typename CurMPtr>
    inline bool doSearchLast(const int depth, const Board &board, const MovePtr curMovePtr) {
        static_fail_undefined_template;
    }

    template<typename CurMPtr>
    inline static bool enumerateBoards(const int depth, const int, const Board &board, MoveManager &mManager,
        Board(&nextBoards)[BOARD_WIDTH], int &invalidMoves, int &finishedMoves);

    template<class MM>
    inline static continueEval doMoveEvaluation(MM &mManager, const int i) {
        static_fail_undefined_template;
    };

    template<class MM>
    inline static bool doHeuristicEvaluation(MM &mManager, const Board &board, const int i, int &finishedMoves) {
        static_fail_undefined_template;
    }

#if ET

    template <typename TData>
    inline bool acquireMove(int &outBoardIdx, TDataPtr* &outMove, const uint threadId, const int depth,
        const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
        Cache &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies);

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

    void searchThreaded(const int depth, const Board &board, MoveManagerT &bmm, double progress,
        const double nextProgressChunk, const Board(&nextBoards)[BOARD_WIDTH], Cache &cache, const int maxBoards,
        const array<int, BOARD_WIDTH> &moveIndicies);

#endif // ENABLE_THREADING
#pragma endregion

#pragma region Variable declarations
    constexpr int PARENT_THREAD_ID = MAX_THREADS - 1;
#if ET
    Board allBoards[MAX_THREADS][MAX_MOVE_DEPTH][BOARD_WIDTH];
    atomic<bool> threadDepthFinished[MAX_MOVE_DEPTH];

    std::mutex threadFinishedLock;
    std::condition_variable threadFinishedCV;

    std::mutex threadsAvailableLock;
    std::condition_variable threadsAvailableCV;
#else // ET
    Board boards[MAX_MOVE_DEPTH][BOARD_WIDTH];
#endif // ET

#pragma endregion
}

#pragma region SearchTree member definitions
const SearchTree::SearchTreeState& SearchTree::getSearchTreeState() {
    return state;
}

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
#else // ET
    for (int i = 0; i < BOARD_SIZE; ++i) {
        state.currentMoveRoute[i] = -1;
    }
#endif // ET

    initMoveCache();
    /*gc::initGC();
    progressPrinter::beginPrintProgress();*/

    Board b = Board().addPiece(3);

    int maxDepthRequired = 0;
    RedMovePtr dummy = RedMovePtr(new RedMoveData(true));

#if ET
    if (!dummy->acquire(0)) {
        throw exception("could not acquire first move.....?");
    }
#endif // ET

    search<RedMovePtr>(0, PARENT_THREAD_ID, b, 0, 1.0, dummy);
}

namespace {
    // enumerate board hashes
    // check if they've already been calculated
    // if not, generate the board and add it to our MoveManagerT
    template<typename CurMPtr>
    inline bool enumerateBoards(const int depth, const int maxBoards, const Board &board, MoveManager &mManager,
        Board(&nextBoards)[BOARD_WIDTH], int &invalidMoves, int &finishedMoves) {
        constexpr bool isRedTurn = is_same<CurMPtr, RedMovePtr>::value;
        typedef conditional<isRedTurn, BlackMovePtr, RedMovePtr>::type NextMPtr;
        typedef conditional<isRedTurn, RedMoveManager, BlackMoveManager>::type MM;

        const int nextDepth = depth + 1;
        Cache &nextCache = *moveCaches[nextDepth];
        MM &mm = static_cast<MM&>(mManager);

        const Heuristic DONT_RECALCULATE_HEURISTIC = (depth % 2 == 0) ? BLACK_WON_HEURISTIC : CATS_GAME_HEURISTIC;
        bool canEndOnCompletion = false;

        ettgc(lGuard lg(moveCacheMutexes[nextDepth]));
        for (int i = 0; i < maxBoards; ++i) {
            if (!board.isValidMove(i)) {
                ++invalidMoves;
                mManager[i] = NextMoveDescriptor(i, 0, DONT_RECALCULATE_HEURISTIC, nullptr);
                continue;
            }

            BoardHash nextHash = board.getNextBoardHash(i);
            mManager[i].col = i;
            mManager[i].hash = nextHash;

            auto it = nextCache.find(nextHash);
            if (it != nextCache.end()) {
                ++stats.cacheHits;
                it->second->incRefCount();

                mManager[i].move = rpc(NextMPtr, it->second);

                if (it->second->getIsFinished()) {
                    ++finishedMoves;
                    mManager[i].heur = DONT_RECALCULATE_HEURISTIC;

                    switch (doMoveEvaluation(mm, i)) {
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
                    ++state.movesAddedSinceLastGC;
                }

#if ET | TGC
                mManager.addMove(i, nextBoard, lg);
#else // ET | TGC
                mManager.addMove(i, nextBoard);
#endif // ET |TGC
                if (doHeuristicEvaluation(mm, nextBoard, i, finishedMoves)) {
                    return true;
                }
            }
        }

        return canEndOnCompletion;
    }

    template <>
    inline static continueEval doMoveEvaluation<BlackMoveManager>(BlackMoveManager &mManager, const int i) {
        RedMovePtr mv = dpc(RedMovePtr, mManager[i].move);
        assert(mv->getIsFinished());

        if (mv->getBlackLost()) {
            return continueEval::unfinishedEval;
        }

        int mtw = mv->getMovesToWin() + 1;
        if (mtw < mManager.movesToWin) {
            mManager.bestMove = mManager[i].col;
            mManager.movesToWin = mtw;
        }

        return mtw == 2 ? continueEval::finishedAfterCheckingNewBoards : continueEval::unfinishedEval;
    }

    template <>
    inline static continueEval doMoveEvaluation<RedMoveManager>(RedMoveManager &mManager, const int i) {
        BlackMovePtr mv = static_cast<BlackMovePtr>(mManager[i].move);
        assert(mv->getIsFinished());

        if (mv->getBlackLost()) {
            mManager.movesToWin = BLACK_LOST;
            return continueEval::finished;
        }

        int mtw = mv->getMovesToWin() + 1;
        if (mtw > mManager.movesToWin) {
            mManager.movesToWin = mtw;
        }

        return continueEval::unfinishedEval;
    }

    template<>
    inline static bool doHeuristicEvaluation<BlackMoveManager>(BlackMoveManager &mManager, const Board& board, const int i, int &finishedMoves) {
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
    inline static bool doHeuristicEvaluation<RedMoveManager>(RedMoveManager &mManager, const Board& board, const int i, int &finishedMoves) {
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
    inline bool doSearchLast<RedMovePtr>(const int depth, const Board &board, const MovePtr curMovePtr) {
        assert(depth < LAST_MOVE_DEPTH);
        return false;
    }

    template <>
    inline bool doSearchLast<BlackMovePtr>(const int depth, const Board &board, const MovePtr curMovePtr) {
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
    inline bool search(const int depth, const uint threadId, const Board &board, double progress, const double progressChunk, CurMPtr curMovePtr)
    {
        constexpr bool isRedTurn = is_same<CurMPtr, RedMovePtr>::value;
        typedef conditional<isRedTurn, RedMoveManager, BlackMoveManager>::type MM;
        typedef conditional<isRedTurn, BlackMovePtr, RedMovePtr>::type NextMPtr;

        assert((depth % 2 == 0) == isRedTurn);
        
        const int nextDepth = depth + 1;

        ++state.movesEvaluated;

        if (doSearchLast<CurMPtr>(depth, board, curMovePtr)) {
            return true;
        }

        assert(depth < LAST_MOVE_DEPTH);

#if !TGC
        if (gc::manageMemory(depth, state.movesAddedSinceLastGC, state.lowestDepthEvaluatedSinceLastGC, state.lowestDepthEvaluated)) {
            state.lowestDepthEvaluatedSinceLastGC = MAX_MOVE_DEPTH;
            state.movesAddedSinceLastGC = 0;
        }
#endif // !TGC

        assert(curMovePtr != nullptr);
        assert(!curMovePtr->getIsFinished());
        etassert(curMovePtr->getWorkerThread() == threadId);

        LowestDepthManager ldm{ depth };
        const int maxBoards = board.symmetryBF.isSymmetric ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
        Board (&nextBoards)[BOARD_WIDTH] = boards[nextDepth];
        Cache &nextCache = *moveCaches[nextDepth];

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
        MM mm(depth, board, curMovePtr); 
        MoveManager &mManager = mm;

        if (enumerateBoards<CurMPtr>(depth, maxBoards, board, mManager, nextBoards, invalidMoves, finishedMoves)) {
            if (isRedTurn) {
                assert(mManager.movesToWin == BLACK_LOST);
            }
            else {
                assert(mManager.movesToWin == 0 || mManager.movesToWin == 2);
            }

            double tmp = state.progressSaved.load();
            while (!state.progressSaved.compare_exchange_weak(tmp, tmp + progressChunk));

            return true;
        }

        const int newBoards = maxBoards - finishedMoves - invalidMoves;

        if (depth < DEPTH_COUNTS_TO_TRACK) {
            state.maxMovesAtDepth[depth] = newBoards;
            state.movesFinishedAtDepth[depth] = 0;

            //for (int i = depth + 1; i < DEPTH_COUNTS_TO_TRACK; ++i) {
            //    state.maxMovesAtDepth[i] = -1;
            //    state.movesFinishedAtDepth[i] = -1;
            //}
        }

        if (finishedMoves > 0) {
            const double finishedProgress = finishedMoves / double(maxBoards - invalidMoves) * progressChunk;
            double tmp = state.progressSaved.load();
            while (!state.progressSaved.compare_exchange_weak(tmp, tmp + finishedProgress));
        }

        if (finishedMoves + invalidMoves == maxBoards) { // all moves have been previously evaluated
            return true;
        }

        const double nextProgressChunk = progressChunk / newBoards;

        mManager.sortByHeuristics();

        // TODO: change to using a MoveManager::iterator
        for (int i = 0; i < newBoards; ++i) {
            int nextBoardIdx = mManager[i].col;
            assert(board.validMoves.cols[nextBoardIdx]);
            NextMPtr nextMovePtr = rpc(NextMPtr, mManager[i].move);
            assert(nextMovePtr != nullptr);
            assert(!nextMovePtr->getIsFinished()); // may need to change this for threading--hard to say

            const Board &nextBoard = nextBoards[nextBoardIdx];

            state.currentMoveRoute[depth] = nextBoardIdx;

            bool finished = search<NextMPtr>(nextDepth, threadId, nextBoard, progress, nextProgressChunk, nextMovePtr);

            state.currentMoveRoute[depth] = -1;

#if ET
            if (!finished) {
                // thread aborted
                curMovePtr->releaseWithoutFinish();
                return -1;
            }
#else
            assert(finished);
#endif

            switch (doMoveEvaluation(mm, i)) { // intentionally use i here rather than nextBoardIdx
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
    inline bool searchLast(const Board &board, const MovePtr curMovePtr) {
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
                bspc(curMovePtr)->setNextHash(lastBoard.boardHash);
                curMovePtr->finish(0);
                return true;
            }
        }

        MovePtr tmp = curMovePtr->getBlackLostMove();
        auto it = *moveCaches[DEPTH]->find(board.boardHash);
#if USE_SMART_POINTERS
        it.second->reset(tmp);
#else // USE_SMART_POINTERS
        delete it.second;
        it.second = tmp;
#endif // USE_SMART_POINTERS
        return true;
    }

#pragma endregion

#pragma region Function definitions

    inline void printParentThreadShortCircuited(int threadId) {
#if VERBOSE
        static_fail(TODO_VERBOSE);
        ulGuard(ioMutex);

        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
        cout << "Parent node has been evaluated.  Thread " << threadId << " can return without finishing.\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#endif // VERBOSE
    }

#if ET
    static_fail("TODO: Remove this and move it to MoveManagerT::iterator");
    // returns true if a moves was acquired, else false (either becauses all moves are finished or the thread was interrupted
    // waits if all next moves are currently locked but at least one is unfinished
    template <typename TData>
    inline bool acquireMove(int &outBoardIdx, TDataPtr* &outMove, const uint threadId, const int depth,
        const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
        Cache &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies) {

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
            ettgc(ulGuard(moveCacheMutexes[nextDepth]));
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
    static_fail("TODO: Remove this and move it to MoveManagerT::iterator");
    const bool acquireThreadId(int &childThreadId) {
        et(ulGuard(threadsAvailableLock));
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
            ulGuard(ioMutex);
            cout << "Thread " << childThreadId << " working on move " << boardMove << "\n";
        }
#endif

        try {
            int val = searchRed(nextDepth, childThreadId, depth, nextBoard, 0.0, nextProgressChunk, nextMovePtr);

#if VERBOSE

            {
                ulGuard(ioMutex);
                cout << "Thread " << childThreadId << " finished working on move " << boardMove << ": " << val << "\n";
            }
#endif

            uLock lk(threadFinishedMutex);
            threadFinishedCV.wait(lk, [] { return true; });
            threadFinished[childThreadId] = true;
            lk.unlock();

#if VERBOSE
            {
                ulGuard(ioMutex);
                cout << "Thread " << childThreadId << " set thread finished\n";
            }
#endif

            threadFinishedCV.notify_one();

            return val;
        }
        catch (exception &e) {
            ulGuard(ioMutex);
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
            cout << "Thread " << childThreadId << " failed with exception:\n" << e.what() << "\n\n";
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);

            return -2;
        }
    }

    void searchThreaded(const int depth, const Board &board, MoveManagerT &bmm, double progress,
        const double nextProgressChunk, const Board(&nextBoards)[BOARD_WIDTH], Cache &cache, const int maxBoards,
        const array<int, BOARD_WIDTH> &moveIndicies)
    {
        assert(depth == THREAD_AT_DEPTH);

#if VERBOSE
        static_fail(TODO_VERBOSE);
        {
            ulGuard(ioMutex);
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
                    et(ulGuard(threadsAvailableMutex));
                    threadsAvailable[childThreadId] = true;
                    break;
                }
            }

#if VERBOSE

            {
                ulGuard(ioMutex);
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
                cout << "Spawned " << MAX_THREADS - threadsAvailable.count() << " thread(s)\n\n";
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
            }
#endif

            {
                et(ulGuard(threadsAvailableMutex));
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
                        ulGuard(ioMutex);
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
            ulGuard(ioMutex);
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 11);
            cout << "Threads joined\n\n";
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
        }
#endif
    }

#endif
#pragma endregion
    //
    //void SearchTree::ThreadLocks::decRedNextRefs(RedMovePtr &ptr, const int depth)
    //{
    //    etassert(moveCacheLocks[depth + 1].owns_lock());
    //    sgc(etassert(garbageLocks[depth + 1].owns_lock()));
    //    int size;
    //    const BoardHash* hashes = ptr->getNextHashes(size);
    //    Cache &cache = *moveCaches[depth + 1];
    //
    //#if SGC
    //    Garbage &garbage = cal.garbage[depth + 1];
    //    for (int i = 0; i < size; ++i) {
    //        if (hashes[i] != 0) {
    //            if (cache.at(hashes[i])->decRefCount() == 0) {
    //                auto added = garbage.insert(hashes[i]);
    //                assert(added.second);
    //            }
    //        }
    //    }
    //#else // SGC
    //    for (int i = 0; i < size; ++i) {
    //        if (hashes[i] != 0) {
    //            cache.at(hashes[i])->decRefCount();
    //        }
    //    }
    //#endif // SGC
    //}
}