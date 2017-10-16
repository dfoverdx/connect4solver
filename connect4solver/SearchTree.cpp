#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <conio.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <time.h>
#include <Windows.h>
#include <Psapi.h> // must go after Windows.h for some reason
#include "MoveData.h"
#include "SearchTree.h"

#ifdef ALWAYS_USE_HEURISTIC
#include "SortingNetwork.h"
#endif

#ifdef MAP
#include <map>
#else
#include <unordered_map>
#endif

using namespace std;
using namespace connect4solver;
using namespace std::chrono;
using namespace std::chrono_literals;

#define cout std::cout

#ifdef ALWAYS_USE_HEURISTIC
#define gg(board) (board).getGameOver()
#define heur(board) (board).heuristic
#else
#define gg(board) (board).gameOver
#define heur(board) (board).getHeuristic()
#endif 

#ifdef MAP
#define cacheMap map
#else
#define cacheMap unordered_map
#endif

typedef cacheMap<BoardHash, BlackMovePtr> BlackCache;
typedef cacheMap<BoardHash, RedMovePtr> RedCache;
typedef unique_lock<mutex> uLock;
typedef BlackCache::iterator BlackCacheIt;
typedef RedCache::iterator RedCacheIt;

int searchBlack(const int depth, const uint threadId, const int parentThreadDepth, const Board &board, double progress, 
                const double progressChunk, BlackMovePtr &curMovePtr);
int searchRed(const int depth, const uint threadId, const int parentThreadDepth, const Board &board, double progress, 
              const double progressChunk, RedMovePtr &curMovePtr);
inline void decRedMoveRefCounts(const RedMovePtr &curMovePtr, BlackCache &nextMoves);

template <typename TData>
inline bool acquireMove(int &nextBoardIdx, shared_ptr<TData>* &outMove, const uint threadId, const int actualDepth,
    const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
    cacheMap<BoardHash, shared_ptr<TData>> &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies);

template <typename TData>
inline bool shortCircuit(shared_ptr<TData>) {
    throw exception("Somehow tried to call shortCircuit() with something other than a move pointer");
}

template <>
inline bool shortCircuit(BlackMovePtr ptr) {
    return ptr == nullptr;
}

template <>
inline bool shortCircuit(RedMovePtr ptr) {
    return ptr != nullptr && ptr->getMovesToWin() == 2;
}

template<typename TPtr>
inline void finishMove(int depth, int movesTilWin, TPtr* curMovePtr);

inline void printProgress(const uint threadId, const Heuristic heuristic);
inline void printTime(const double progress);

#define setBlackLost(curMovePtr) curMovePtr = nullptr; return BLACK_LOST
#define setProgress(progress) threadProgress[parentThreadDepth == -1 ? threadId : 2] = progress

size_t garbageCollect();
inline void manageMemory();
void setMaxMemoryToUse();

const clock_t begin_time = clock();

struct CacheStats {
    ull hits;
    ull misses;
};

CacheStats cacheStats = CacheStats();
atomic<ull> tmpBoardsEvaluated = 0;
atomic<ull> boardsEvaluated = 0;
atomic<ull> movesAddedSinceLastGC = 0;
ull movesAllowedToBeAddedAfterGarbageCollection = 0;
ull totalGarbageCollected = 0;
MEMORYSTATUSEX memInfo;
tinyint cacheFullAt = -1;
HANDLE hProcess = GetCurrentProcess();
PROCESS_MEMORY_COUNTERS pmc;
size_t maxMemoryToUse;

Board blackBoards[MAX_THREADS][MAX_MOVE_DEPTH / 2][BOARD_WIDTH];
Board redBoards[MAX_THREADS][MAX_MOVE_DEPTH / 2][BOARD_WIDTH];
BlackCache blackMoves[MAX_MOVE_DEPTH / 2];
RedCache redMoves[MAX_MOVE_DEPTH / 2];
mutex cacheLocks[MAX_MOVE_DEPTH];
condition_variable moveCVs[MAX_MOVE_DEPTH];
mutex ioLock;

atomic<bool> threadDepthFinished[MAX_MOVE_DEPTH * 2];

array<array<tinyint, BOARD_SIZE>, MAX_THREADS> currentMoveRoute;
double threadProgress[MAX_THREADS + 1];

#ifdef ALWAYS_USE_HEURISTIC
const array<int, BOARD_WIDTH> MOVE_INDICES = { 0, 1, 2, 3, 4, 5, 6 };
#else
const array<int, BOARD_WIDTH> MOVE_INDICES = { 3, 2, 1, 0, 4, 5, 6 };
#endif

void SearchTree::run()
{
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    setMaxMemoryToUse();

    for (int i = 0; i < MAX_MOVE_DEPTH / 2; ++i) {
        blackMoves[i] = BlackCache();
        redMoves[i] = RedCache();
        threadDepthFinished[i * 2] = false;
        threadDepthFinished[i * 2 + 1] = false;
    }

    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < MAX_THREADS; ++j) {
            currentMoveRoute[j][i] = -1;
        }
    }

    for (int i = 0; i <= MAX_THREADS; ++i) {
        threadProgress[i] = 0.0;
    }

    Board b = Board().addPiece(3);

    int maxDepthRequired = 0;
    RedMovePtr dummy = RedMovePtr(new RedMoveData(true));
    if (!dummy->acquire(0)) {
        throw exception("could not acquire first move.....?");
    }

    int depthRequired = searchRed(0, 0, -1, b, 0, 1.0, dummy);
    if (depthRequired == BLACK_LOST) {
        cout << "Somehow root depth found black lost\n";
    }
    else {
        cout << depthRequired << "\n";
    }
}

// returns moves til win:
//   -1: thread was interupted and move was left unfinished
//   0 to BLACK_LOST - 1: finished search and black wins
//   BLACK_LOST: finished search and either red wins or it's a cats game
int searchBlack(const int depth, const uint threadId, const int parentThreadDepth, const Board &board, double progress, 
                const double progressChunk, BlackMovePtr &curMovePtr) {
    assert(depth < MAX_MOVE_DEPTH);

    const int depthIdx = depth / 2;
    const int nextDepth = depth + 1;
    const int nextDepthIdx = depth / 2 + 1;
    const int maxBoards = board.isSymmetric ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
    const double nextProgressChunk = progressChunk / maxBoards;
    Board (&nextBoards)[BOARD_WIDTH] = redBoards[threadId][nextDepthIdx];
    RedCache &nextMoves = redMoves[nextDepthIdx];

    assert(curMovePtr != nullptr);
    assert(!curMovePtr->getIsFinished());
    assert(curMovePtr->getWorkerThread() == threadId);

    setProgress(progress);

    // if this is the max depth, check all next possible boards
    // if winning board found, finish this move and return
    if (nextDepth >= MAX_MOVE_DEPTH) {
        for (int i = 0; i < maxBoards; ++i) {
            if (!board.isValidMove(i)) {
                continue;
            }

            Board lastBoard = board.addPiece(i);
            if (gg(lastBoard) == gameOverState::blackWon) {
                curMovePtr->setNextHash(lastBoard.boardHash);
                curMovePtr->finish(0);
                return 0;
            }
            else {
                continue;
            }
        }

        setBlackLost(curMovePtr);
    }

    int movesTilWin = BLACK_LOST;
    int bestMove = -1;
    BoardHash bestHash = 0;
    int newBoards = 0;
    int movesFinished = 0;
    Board* next = nullptr;

    array<Heuristic, BOARD_WIDTH> heuristics;
    array<int, BOARD_WIDTH> indicies = MOVE_INDICES;

    // enumerate boards
    // if winning board found, finish this move and return movesTilWin = 0
    for (int i = 0; i < maxBoards; ++i) {
        if (!board.isValidMove(i)) {
            heuristics[i] = BLACK_LOST_HEURISTIC;
            continue;
        }

        next = &nextBoards[i];
        *next = board.addPiece(i);
        ++tmpBoardsEvaluated;

        switch (gg(*next)) {
        case gameOverState::blackLost:
        case gameOverState::unfinished:
            heuristics[i] = heur(*next);
            continue;

        case gameOverState::blackWon:
            finishMove(depth, 0, &curMovePtr);
            return 0;
        }
    }

    RedMovePtr* localMoves[BOARD_WIDTH];
    int localMovesCounter = 0;

    // search for moves in the cache
    // if not found, add them
    {
        lock_guard<mutex> lk(cacheLocks[nextDepth]);
        for (int i = 0; i < maxBoards; ++i)
        {
            if (!board.isValidMove(i)) {
                ++movesFinished;
                continue;
            }

            next = &nextBoards[i];
            RedCacheIt it = nextMoves.find(next->boardHash);
            if (it == nextMoves.end()) {
                ++cacheStats.misses;
                ++movesAddedSinceLastGC;
                localMoves[localMovesCounter] = new RedMovePtr(new RedMoveData(next->isSymmetric));
                nextMoves[next->boardHash] = *localMoves[localMovesCounter];
                ++localMovesCounter;
            }
            else {
                ++cacheStats.hits;
                if (it->second == nullptr) {
                    // black lost
                    continue;
                }

                if (it->second->getIsFinished()) {
                    ++movesFinished;
                    int mtw = it->second->getMovesToWin() + 1;
                    if (mtw < movesTilWin) {
                        if (bestMove != -1) {
                            RedMovePtr lastPtr = nextMoves[bestHash];
                            if (lastPtr != nullptr) {
                                lastPtr->decRefCount();
                            }
                        }

                        // short-circuit if black can force a win with its next move
                        if (mtw == 2) {
                            curMovePtr->setNextHash(bestHash);
                            finishMove(depth, 2, &curMovePtr);

                            for (int j = 0; j < localMovesCounter; ++j) {
                                (*localMoves[j])->decRefCount();
                                localMoves[j]->reset();
                            }

                            return 2;
                        }

                        bestMove = i;
                        bestHash = next->boardHash;
                        movesTilWin = mtw;
                        it->second->incRefCount();
                    }
                }
            }
        }
    }

    for (int i = 0; i < localMovesCounter; ++i) {
        localMoves[i]->reset();
        localMoves[i] = nullptr;
    }

    localMovesCounter = 0;

    // all moves have been previously evaluated
    if (movesFinished == maxBoards) {
        if (movesTilWin == BLACK_LOST) {
            setBlackLost(curMovePtr);
        }

        assert(bestMove != -1);
        finishMove(depth, movesTilWin, &curMovePtr);
        return movesTilWin;
    }

    if (maxBoards == BOARD_WIDTH) {
        SortingNetwork::Sort7Reverse(heuristics, indicies);
    }
    else {
        SortingNetwork::Sort4Reverse(heuristics, indicies);
    }

    progress += nextProgressChunk * movesFinished;
    setProgress(progress);

    RedMovePtr* nextMovePtr = nullptr;
    int nextBoardIdx;
    
    // get move to work on
    while (acquireMove(nextBoardIdx, nextMovePtr, threadId, depth, parentThreadDepth, board, nextBoards, nextMoves, 
                       maxBoards, indicies)) { 
        assert(nextMovePtr != nullptr && *nextMovePtr != nullptr);

        Board &next = nextBoards[nextBoardIdx];
        currentMoveRoute[threadId][depth] = nextBoardIdx;
        int tmpMovesTilWin = searchRed(nextDepth, threadId, parentThreadDepth, next, progress, nextProgressChunk, *nextMovePtr);
        currentMoveRoute[threadId][depth] = -1;

        if (tmpMovesTilWin != BLACK_LOST) {
            ++tmpMovesTilWin;
            if (tmpMovesTilWin < movesTilWin) {
                if (bestMove != -1) {
                    localMoves[localMovesCounter] = &nextMoves[bestHash];
                    ++localMovesCounter;
                }

                movesTilWin = tmpMovesTilWin;
                bestMove = nextBoardIdx;
                bestHash = next.boardHash;
            }
            else {
                localMoves[localMovesCounter] = nextMovePtr;
                ++localMovesCounter;
            }

            // short-circuit if black can force a win with its next move
            if (movesTilWin == 2) {
                for (int i = 0; i < localMovesCounter; ++i) {
                    (*localMoves[i])->decRefCount();
                    localMoves[i]->reset();
                }

                curMovePtr->setNextHash(bestHash);
                finishMove(depth, 2, &curMovePtr);
                return 2;
            }
        }
        else {
            assert(*nextMovePtr == nullptr);
        }

        progress += nextProgressChunk;
        setProgress(progress);
    }

    if (nextBoardIdx == -1) {
        // thread was aborted because parent thread was notified
        assert(nextMovePtr == nullptr);
        curMovePtr->releaseWithoutFinish();
        return -1;
    }

    manageMemory();

    if (depth <= DEPTH_TO_PRINT_PROGRESS) {
        printProgress(threadId, board.heuristic);
    }

    if (bestMove == -1) {
        assert(localMovesCounter == 0);
        setBlackLost(curMovePtr);
    }

    for (int i = 0; i < localMovesCounter; ++i) {
        (*localMoves[i])->decRefCount();
        localMoves[i]->reset();
    }

    curMovePtr->setNextHash(bestHash);
    curMovePtr->finish(movesTilWin);

    return movesTilWin;
}

// returns moves til win:
//   -1: thread was interupted and move was left unfinished
//   0 to BLACK_LOST - 1: finished search and black wins
//   BLACK_LOST: finished search and either red wins or it's a cats game
int searchRed(const int depth, const uint threadId, const int parentThreadDepth, const Board &board, double progress,
              const double progressChunk, RedMovePtr &curMovePtr) {
    const int depthIdx = depth / 2;
    const int nextDepth = depth + 1;
    const int nextDepthIdx = depth / 2; // don't increment depth idx into black board array
    const int maxBoards = board.isSymmetric ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
    const double nextProgressChunk = progressChunk / maxBoards;
    Board(&nextBoards)[BOARD_WIDTH] = blackBoards[threadId][nextDepthIdx];
    BlackCache &nextMoves = blackMoves[nextDepthIdx];

    assert(curMovePtr != nullptr);
    assert(!curMovePtr->getIsFinished());
    assert(curMovePtr->getWorkerThread() == threadId);

    setProgress(progress);

    int movesTilWin = 0;
    BoardHash bestHash = 0;
    int newBoards = 0;
    int movesFinished = 0;
    Board* next = nullptr;

    array<Heuristic, BOARD_WIDTH> heuristics;
    array<int, BOARD_WIDTH> indicies = MOVE_INDICES;

    // enumerate boards
    // if losing board found, set black lost
    for (int i = 0; i < maxBoards; ++i) {
        if (!board.isValidMove(i)) {
            heuristics[i] = BLACK_WON_HEURISTIC;
            continue;
        }

        next = &nextBoards[i];
        *next = board.addPiece(i);
        ++tmpBoardsEvaluated;

        switch (gg(*next)) {
        case gameOverState::blackWon:
        case gameOverState::unfinished:
            heuristics[i] = heur(*next);
            continue;

        case gameOverState::blackLost:
            decRedMoveRefCounts(curMovePtr, nextMoves);
            setBlackLost(curMovePtr);
        }
    }

    // search for moves in the cache
    // if not found, add them
    {
        lock_guard<mutex> lk(cacheLocks[nextDepth]);
        for (int i = 0; i < maxBoards; ++i)
        {
            if (!board.isValidMove(i)) {
                ++movesFinished;
                continue;
            }

            next = &nextBoards[i];
            curMovePtr->setNextHash(i, next->boardHash);
            BlackCacheIt it = nextMoves.find(next->boardHash);
            if (it == nextMoves.end()) {
                ++cacheStats.misses;
                ++movesAddedSinceLastGC;
                nextMoves[next->boardHash] = BlackMovePtr(new BlackMoveData(next->isSymmetric));
            }
            else {
                ++cacheStats.hits;
                if (it->second == nullptr) {
                    decRedMoveRefCounts(curMovePtr, nextMoves);
                    setBlackLost(curMovePtr);
                }

                it->second->incRefCount();

                if (it->second->getIsFinished()) {
                    ++movesFinished;

                    int mtw = it->second->getMovesToWin() + 1;
                    movesTilWin = max(mtw, movesTilWin);
                }
            }
        }
    }

    // all moves have been previously evaluated
    if (movesFinished == maxBoards) {
        assert(movesTilWin > 0);
        finishMove(depth, movesTilWin, &curMovePtr);
        return movesTilWin;
    }

    if (maxBoards == BOARD_WIDTH) {
        SortingNetwork::Sort7(heuristics, indicies);
    }
    else {
        SortingNetwork::Sort4(heuristics, indicies);
    }

    progress += nextProgressChunk * movesFinished;
    setProgress(progress);

    BlackMovePtr* nextMovePtr = nullptr;
    int nextBoardIdx;

    // get move to work on
    while (acquireMove(nextBoardIdx, nextMovePtr, threadId, depth, parentThreadDepth, board, nextBoards, nextMoves,
                       maxBoards, indicies)) {
        assert(nextMovePtr != nullptr && *nextMovePtr != nullptr);

        Board &next = nextBoards[nextBoardIdx];
        currentMoveRoute[threadId][depth] = nextBoardIdx;
        int tmpMovesTilWin = searchBlack(nextDepth, threadId, parentThreadDepth, next, progress, nextProgressChunk, *nextMovePtr);
        currentMoveRoute[threadId][depth] = -1;

        if (tmpMovesTilWin == BLACK_LOST) {
            decRedMoveRefCounts(curMovePtr, nextMoves);
            setBlackLost(curMovePtr);
        }

        ++tmpMovesTilWin;
        movesTilWin = max(tmpMovesTilWin, movesTilWin);

        progress += nextProgressChunk;
        setProgress(progress);
    }

    if (nextBoardIdx == -1) {
        // thread was aborted because parent thread was notified
        assert(nextMovePtr == nullptr);
        curMovePtr->releaseWithoutFinish();
        return -1;
    }

    manageMemory();

    if (depth <= DEPTH_TO_PRINT_PROGRESS) {
        printProgress(threadId, board.heuristic);
    }

    assert(movesTilWin > 0);
    curMovePtr->finish(movesTilWin);
    return movesTilWin;
}

inline void decRedMoveRefCounts(const RedMovePtr &curMovePtr, BlackCache &nextMoves) {
    const RedHashes hashes = curMovePtr->getNextHashes();
    for (auto rhit = hashes.begin(); rhit != hashes.end(); ++rhit) {
        BlackMovePtr tmp = nextMoves.at(rhit->second);
        if (tmp != nullptr) {
            tmp->decRefCount();
        }
    }
}

// returns true if a moves was acquired, else false (either becauses all moves are finished or the thread was interrupted
// waits if all next moves are currently locked but at least one is unfinished
template<typename TData>
inline bool acquireMove(int &outBoardIdx, shared_ptr<TData>* &outMove, const uint threadId, const int actualDepth,
    const int parentThreadDepth, const Board &curBoard, const Board(&nextBoards)[BOARD_WIDTH],
    cacheMap<BoardHash, shared_ptr<TData>> &cache, const int maxBoards, const array<int, BOARD_WIDTH> &moveIndicies) {

    using TPtr = shared_ptr<TData>;

    const int depth = actualDepth / 2;
    const int actualNextDepth = actualDepth + 1;
    outMove = nullptr;
    outBoardIdx = -1;

    int movesFinished = 0;

    // check for unfinished, unlocked moves
    for (int i = 0; i < maxBoards; ++i) {
        int x = moveIndicies[i];
        if (!curBoard.isValidMove(x)) {
            ++movesFinished;
            continue;
        }

        TPtr* tmpMvPtr = &cache.at(nextBoards[x].boardHash);
        if (shortCircuit(*tmpMvPtr)) {
            outMove = tmpMvPtr;
            outBoardIdx = i;
            return false;
        }

        if ((*tmpMvPtr) == nullptr || (*tmpMvPtr)->getIsFinished()) {
            ++movesFinished;
            continue;
        }

        bool acquiredMove = (*tmpMvPtr)->acquire(threadId);
        if (acquiredMove) {
            outBoardIdx = x;
            outMove = tmpMvPtr;
            return true;
        }
    }

    if (movesFinished == maxBoards) {
        outBoardIdx = 0;
        return false;
    }

    // all unfinished moves are locked
    for (int i = 0; i < maxBoards; ++i) {
        if (!curBoard.isValidMove(i)) {
            continue;
        }

        BoardHash nHash = nextBoards[i].boardHash;

        uLock lk(cacheLocks[actualNextDepth]);
        duration<uint> timeWaited = 0s;
        while (!moveCVs[depth].wait_for(lk, TIME_TO_WAIT_FOR_CVS, [cache, depth, nHash] {
            TPtr tmpMvPtr = cache.at(nHash);
            if (tmpMvPtr == nullptr || tmpMvPtr->getIsFinished()) {
                return true;
            }
            else {
                return false;
            }
        })) {


            timeWaited += duration_cast<seconds>(TIME_TO_WAIT_FOR_CVS);
            lock_guard<mutex> ioLk(ioLock);
            cout << "Thread " << threadId <<
                " at depth " << actualDepth <<
                " has waited " << timeWaited.count() <<
                " seconds for a move to finish.\n";

            if (threadDepthFinished[parentThreadDepth]) {
                cout << "Parent node has been evaluated.  Thread " << threadId << " can return without finishing.\n";
                lk.unlock();
                outMove = nullptr;
                return false;
            }
        }

        TPtr* tmpMvPtr = &cache.at(nHash);
        if (shortCircuit(*tmpMvPtr)) {
            outMove = tmpMvPtr;
            outBoardIdx = i;
            lk.unlock();
            return false;
        }

        lk.unlock();
    }

    return false;
}

inline void printProgress(const uint threadId, const Heuristic heuristic)
{
    ioLock.lock();
    double progress = 0.0;
    for (int i = 0; i <= MAX_THREADS; ++i) {
        progress += threadProgress[i];
    }

    cout << "Progress: " << fixed << setprecision(10) << progress * 100 << "%\n"
        << "Boards evaluated: " << (boardsEvaluated + tmpBoardsEvaluated) << '\n'
        << "Cache hits: " << cacheStats.hits << "\nCache misses: " << cacheStats.misses << '\n'
        << "Cache size: " << cacheStats.misses - totalGarbageCollected << '\n';

    printTime(progress);

    cout << "\nMove route:\n";
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (currentMoveRoute[threadId][i] == -1) {
            break;
        }

        cout << setfill(' ') << setw(4) << static_cast<int>(currentMoveRoute[threadId][i]);
    }

    cout << "\nCurrent Heuristic: " << heuristic;
    cout << "\n\n";
    ioLock.unlock();
}

inline void printTime(const double progress) {
    ull ms = clock() - begin_time;
    ull seconds = ms / CLOCKS_PER_SEC;
    ull minutes = seconds / 60;
    ull hours = minutes / 60;
    ull days = hours / 24;

    cout << "Time elapsed:   " << setfill(' ') << setw(8) << days << ':'
        << setfill('0') << setw(2) << hours % 24 << ':'
        << setfill('0') << setw(2) << minutes % 60 << ':'
        << setfill('0') << setw(2) << seconds % 60 << "\n";

    ms = static_cast<ull>(ms / progress);
    seconds = ms / CLOCKS_PER_SEC - seconds;
    minutes = seconds / 60;
    hours = minutes / 60;
    days = hours / 24;

    cout << "Time remaining: " << setfill(' ') << setw(8) << days << ':'
        << setfill('0') << setw(2) << hours % 24 << ':'
        << setfill('0') << setw(2) << minutes % 60 << ':'
        << setfill('0') << setw(2) << seconds % 60 << "\n";
}

template<typename TPtr>
inline void finishMove(int depth, int movesTilWin, TPtr* curMovePtr) {
    assert(curMovePtr != nullptr);
    uLock lk(cacheLocks[depth]);
    
    if (movesTilWin == BLACK_LOST) {
        *curMovePtr = nullptr;
    }
    else {
        (*curMovePtr)->finish(movesTilWin);
    }

    lk.unlock();
    moveCVs[depth].notify_all();
}

ull garbageCollect()
{
    cout << "Collecting garbage\n";

    //#ifndef NDEBUG
    ull movesWithMultipleReferences = 0;
    //#endif

    const int maxDepth = MAX_MOVE_DEPTH / 2;
    unordered_set<BoardHash> redS[maxDepth];
    unordered_set<BoardHash> blackS[maxDepth];
    unordered_set<BoardHash> &rs = redS[0];
    unordered_set<BoardHash> &bs = blackS[0];

    ull movesDeleted = 0;
    ull movesCondensed = 0;
    for (int d = 0; d < maxDepth; ++d) {
        rs = redS[d];
        bs = blackS[d];

        {
            lock_guard<mutex> lg(cacheLocks[d * 2]);
            for (RedCacheIt it = redMoves[d].begin(); it != redMoves[d].end(); ++it) {
                if (it->second != nullptr) {
                    if (it->second->getRefCount() == 0) {
                        assert(it->second->getWorkerThread() == 0x7);
                        rs.insert(it->first);

                        //#ifndef NDEBUG
                        if (it->second.use_count() > 1) {
                            ++movesWithMultipleReferences;
                        }
                        //#endif
                    }
                    else if (it->second->getIsFinished()) {
                        movesCondensed += it->second->condense();
                    }
                }
            }

#ifdef VERBOSE
            if (rs.size() > 0) {
                cout << "Erasing " << rs.size() << " red moves at depth=" << static_cast<int>(d) << '\n';
            }
#endif

            for (unordered_set<BoardHash>::iterator rsit = rs.begin(); rsit != rs.end(); ++rsit) {
                if (((d * 2) < maxDepth - 1) & (*rsit != 0)) {
                    RedHashes hashes = redMoves[d][*rsit]->getNextHashes();
                    for (auto rhit = hashes.begin(); rhit != hashes.end(); ++rhit) {
                        BlackMovePtr mptr = blackMoves[d][rhit->second];
                        if (mptr != nullptr) {
                            mptr->decRefCount();
                        }
                    }
                }

                redMoves[d].erase(*rsit);
            }
        }

        movesDeleted += rs.size();
        rs.clear();
        if (d < maxDepth - 1) {
            rs = redS[d + 1];
        }

        {
            lock_guard<mutex> lg(cacheLocks[d * 2 + 1]);
            for (BlackCacheIt it = blackMoves[d].begin(); it != blackMoves[d].end(); ++it) {
                if (it->second != nullptr && it->second->getRefCount() == 0) {
                    assert(it->second->getWorkerThread() == 0x7);
                    bs.insert(it->first);

                    //#ifndef NDEBUG
                    if (it->second.use_count() > 1) {
                        ++movesWithMultipleReferences;
                    }
                    //#endif
                }
            }

#ifdef VERBOSE
            if (bs.size() > 0) {
                cout << "Erasing " << bs.size() << " black moves at depth=" << static_cast<int>(d) << '\n';
            }
#endif

            for (unordered_set<BoardHash>::iterator bsit = bs.begin(); bsit != bs.end(); ++bsit) {
                if (((d * 2 + 1) < maxDepth - 1) & (*bsit != 0)) {
                    const BoardHash nextHash = blackMoves[d][*bsit]->getNextHash();
                    if (nextHash != 0) {
                        RedMovePtr mptr = redMoves[d][nextHash];
                        if (mptr != nullptr) {
                            mptr->decRefCount();
                        }
                    }
                }

                blackMoves[d].erase(*bsit);
            }
        }

        movesDeleted += bs.size();
        bs.clear();
    }

    //#ifndef NDEBUG
    if (movesWithMultipleReferences > 0) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
        cout << "\nWarning: found " << movesWithMultipleReferences << " moves with multiple strong references\n";
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    }
    //#endif

    cout << "\nCondensed " << movesCondensed << " red move hashes: " << ((sizeof(BoardHash) * movesCondensed) >> 20) 
        << "MB reclaimed\n";

    //if (movesDeleted >= DATA_TO_DELETE) {
        return movesDeleted;
    //}

#ifdef VERBOSE
    cout << "\nDid not find enough unreferenced moves -- deleting BLACK_LOST moves\n\n";
#else
    cout << "Did not find enough unreferenced moves -- deleting BLACK_LOST moves\n";
#endif

    for (int di = 0; di < maxDepth; ++di) {
        const int d = di < 10 ? di : maxDepth - di + 9;

        rs = redS[d];
        bs = blackS[d];
        {
            lock_guard<mutex> lg(cacheLocks[d * 2]);
            for (RedCacheIt it = redMoves[d].begin(); (it != redMoves[d].end()) & (movesDeleted < DATA_TO_DELETE); ++it) {
                if (it->second == nullptr) {
                    movesDeleted++;
                    rs.insert(it->first);
                }
            }

#ifdef VERBOSE
            if (rs.size() > 0) {
                cout << "Erasing " << rs.size() << " red moves at depth=" << static_cast<int>(d) << '\n';
            }
#endif

            for (unordered_set<BoardHash>::iterator sit = rs.begin(); sit != rs.end(); ++sit) {
                redMoves[d].erase(*sit);
            }
        }

        rs.clear();

        {
            lock_guard<mutex> lg(cacheLocks[d * 2 + 1]);
            for (BlackCacheIt it = blackMoves[d].begin(); (it != blackMoves[d].end()) & (movesDeleted < DATA_TO_DELETE); ++it) {
                if (it->second == nullptr) {
                    movesDeleted++;
                    bs.insert(it->first);
                }
            }


#ifdef VERBOSE
            if (bs.size() > 0) {
                cout << "Erasing " << bs.size() << " black moves at depth=" << static_cast<int>(d) << '\n';
            }
#endif

            for (unordered_set<BoardHash>::iterator sit = bs.begin(); sit != bs.end(); ++sit) {
                blackMoves[d].erase(*sit);
            }
        }

        bs.clear();
    }

    return movesDeleted;
}

inline void manageMemory() {
    if ((tmpBoardsEvaluated >= CHECK_MEMORY_AFTER_BOARDS_EVALUATED) &
        (movesAddedSinceLastGC > movesAllowedToBeAddedAfterGarbageCollection)) {
        boardsEvaluated += tmpBoardsEvaluated;
        tmpBoardsEvaluated = 0;

        GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
        cout << "Current process size: " << (pmc.PagefileUsage >> 20) << "MB / " << (maxMemoryToUse >> 20) << "MB\n\n";

        if (pmc.PagefileUsage > maxMemoryToUse) {
            ull garbageCollected = garbageCollect();
            movesAllowedToBeAddedAfterGarbageCollection = garbageCollected >> 1;
            movesAddedSinceLastGC = 0;
            totalGarbageCollected += garbageCollected;

            cout << "\nTotal moves erased: " << garbageCollected << "\n";
            setMaxMemoryToUse();
        }
    }
}

void setMaxMemoryToUse() {
    GlobalMemoryStatusEx(&memInfo);
    GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));

    maxMemoryToUse = max((memInfo.ullAvailPhys + pmc.WorkingSetSize) * ALLOWED_MEMORY_PERCENT / 100, pmc.WorkingSetSize);

    cout << "Max memory: " << (maxMemoryToUse >> 20) << "MB\n";
    cout << "Current process size: " << (pmc.PagefileUsage >> 20) << "MB\n\n";
}
