#include <cassert>
#include "Macros.h"
#include "MoveCache.h"
#include "MoveManager.h"
#include "SortingNetwork.h"

using namespace std;
using namespace connect4solver;

using Cache = connect4solver::moveCache::Cache;

auto &moveCaches = connect4solver::moveCache::moveCaches;

#if ET | TGC
auto &moveCacheMutexes = connect4solver::moveCache::moveCacheMutexes;
#endif // ET | TGC

#if ET
auto &moveCVs = connect4solver::moveCache::moveCVs;
#endif // ET

NextMoveDescriptor& MoveManager::operator[](int x) {
    return m_moves[x];
}

inline void MoveManager::addMoveInner(const int x, const Board &nextBoard) {
    assertMax(x, MAX_COLUMN);
    assert(validMoves.cols[x]);

    m_moves[x].heur = nextBoard.heuristic;
    
    if (m_moves[x].move == nullptr) {
        Cache &mc = *moveCaches[nextDepth];
        assert(mc.find(nextBoard.boardHash) == mc.end());

        // explicit creation of NextMPtr in case we're using smart pointers
        auto it = mc.insert({ nextBoard.boardHash, makeMovePtr(!!nextBoard.symmetryBF.isSymmetric) });

        m_moves[x].move = it.first->second;
    }
    else {
        // happens when we add a move to the cache and then another board short-circuits the search of the
        // unfinished move
        assert(!m_moves[x].move->getIsFinished());
    }
}

#if ET | TGC
void MoveManager::addMove(const int x, const Board &nextBoard, const uLock &cacheLock) {
    ettgcassert(cacheLock.mutex() == &(moveCacheMutexes[nextDepth]) && cacheLock.owns_lock());
    addMoveInner(x, nextBoard);
}

void MoveManager::addMove(const int x, const Board &nextBoard, const lGuard &cacheLock) {
    addMoveInner(x, nextBoard);
}
#else // ET | TGC
void MoveManager::addMove(const int x, const Board &nextBoard) {
    addMoveInner(x, nextBoard);
}
#endif // ET | TGC

namespace {
    template <typename MPtr>
    inline void finishMoveBlackLost(const int depth, const BoardHash currentHash);
}

BlackMoveManager::BlackMoveManager(const int depth, const Board &curBoard, BlackMovePtr curMovePtr)
    : MoveManager(depth, curBoard.boardHash, curBoard.validMoves, curMovePtr, BLACK_LOST), bestMove(-1) {
    assert(depth % 2 == 1);
}

inline MovePtr BlackMoveManager::makeMovePtr(const bool isSymmetric) {
    return RedMovePtr(new RedMoveData(isSymmetric));
}

inline void BlackMoveManager::finishMove(const BoardHash bestHash) {
    {
        ettgc(ulGuard(moveCacheMutexes[depth]));
        if (movesToWin == BLACK_LOST) {
            assert(bestMove == -1);
            assert(bestHash == 0);

            finishMoveBlackLost<BlackMovePtr>(depth, currentHash);
        }
        else {
            assert(bestMove != -1);
            assert(bestHash != 0);
            bspc(m_curMovePtr)->setNextHash(bestHash);
            bspc(m_curMovePtr)->finish(movesToWin);
        }
    }

    et(moveCVs[depth].notify_all());
}

BlackMoveManager::~BlackMoveManager() {
    // if move was canceled midway through by another thread, don't do anything
    if (movesToWin != -1) {
        BoardHash bestHash = 0;
        {
            ettgc(ulGuard(moveCacheMutexes[depth + 1]));

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                if (m_moves[i].hash == 0) {
                    continue;
                }

                if (m_moves[i].col == bestMove) {
                    bestHash = m_moves[i].hash;
                    continue;
                }

                m_moves[i].move->decRefCount();
            }

            assert(movesToWin == BLACK_LOST || bestHash != 0);
        }

        finishMove(bestHash);
    }
    else {
        assert(ET);
    }
}

inline void BlackMoveManager::sortByHeuristics() {
    if (m_curMovePtr->getIsSymmetric()) {
        SortingNetwork::Sort4Reverse(m_moves);
    }
    else { 
        SortingNetwork::Sort7Reverse(m_moves);
    }
}

RedMoveManager::RedMoveManager(const int depth, const Board &curBoard, RedMovePtr curMovePtr)
    : MoveManager(depth, curBoard.boardHash, curBoard.validMoves, curMovePtr, 0) {
    assert(depth % 2 == 0);
}

inline MovePtr RedMoveManager::makeMovePtr(const bool isSymmetric) {
    return BlackMovePtr(new BlackMoveData(isSymmetric));
}

inline void RedMoveManager::finishMove() {
    {
        ettgc(ulGuard(moveCacheMutexes[depth]));
        if (movesToWin == BLACK_LOST) {
            finishMoveBlackLost<RedMovePtr>(depth, currentHash);
        }
        else {
            copyHashesToMove();
            rspc(m_curMovePtr)->finish(movesToWin);
        }
    }

    et(moveCVs[depth].notify_all());
}

inline void RedMoveManager::copyHashesToMove() {
    for (int i = 0; i < BOARD_WIDTH; ++i) {
        if (m_moves[i].hash == 0) {
            continue;
        }

        rspc(m_curMovePtr)->setNextHash(m_moves[i].col, m_moves[i].hash);
    }
}

RedMoveManager::~RedMoveManager() {
    // if move was canceled midway through by another thread, don't do anything
    if (movesToWin != -1) {
        assert(!m_curMovePtr->getIsFinished());
        if (movesToWin == BLACK_LOST) {
            ettgc(ulGuard(moveCacheMutexes[depth + 1]));

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                if (m_moves[i].hash == 0) {
                    continue;
                }

                m_moves[i].move->decRefCount();
            }
        }

        finishMove();
    }
    else {
        assert(ET);
        ettgc(ulGuard(moveCacheMutexes[depth]));
        copyHashesToMove();
    }
}

inline void RedMoveManager::sortByHeuristics() {
    if (m_curMovePtr->getIsSymmetric()) {
        SortingNetwork::Sort4(m_moves);
    }
    else {
        SortingNetwork::Sort7(m_moves);
    }
}

namespace {
    template <typename MPtr>
    inline void finishMoveBlackLost(const int depth, const BoardHash currentHash) {
        Cache &cache = *moveCaches[depth];
        auto it = cache.find(currentHash);

#if USE_SMART_POINTERS
        it->second.reset(it->second->getBlackLostMove());
#else // USE_SMART_POINTERS
        MPtr tmp = (MPtr)it->second;
        it->second = it->second->getBlackLostMove();
        delete tmp;
#endif // USE_SMART_POINTERS
    }
}