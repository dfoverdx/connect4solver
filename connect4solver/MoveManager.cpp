#include <cassert>
#include "Macros.h"
#include "MoveManager.h"

using namespace std;
using namespace connect4solver;

BlackMoveManager::BlackMoveManager(const int depth, const Board &curBoard, BlackMovePtr* currentMovePtr)
    : MoveManager<BlackMovePtr>(depth, curBoard, currentMovePtr, BLACK_LOST), bestMove(-1) {}

inline void BlackMoveManager::finishMove(const BoardHash bestHash)
{
    {
        ettgc(lGuard(SearchTree::cal.moveCacheLocks[depth]));

        if (movesToWin == BLACK_LOST) {
            assert(bestMove == -1);
            assert(bestHash == 0);
            int oldRefCount = (*currentMovePtr)->getRefCount();
            MoveData::setBlackLost(rpc(MovePtr*, currentMovePtr));

            assert(SearchTree::cal.moveCache[depth].at(currentHash) == *currentMovePtr && *currentMovePtr != nullptr);
            assert((*currentMovePtr)->getRefCount() == oldRefCount);
        }
        else {
            assert(bestMove != -1);
            assert(bestHash != 0);
            (*currentMovePtr)->setNextHash(bestHash);
            (*currentMovePtr)->finish(movesToWin);
        }
    }

    et(SearchTree::cal.moveCVs[depth].notify_all());
}

BlackMoveManager::~BlackMoveManager() {
    // if move was canceled midway through by another thread, don't do anything
    if (movesToWin != -1) {
        BoardHash bestHash = 0;
        {
            ettgc(lGuard(SearchTree::cal.moveCacheLocks[depth + 1]));
            sgc(ettgc(lGuard(SearchTree::cal.garbageLocks[depth + 1])));

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                if (moves[i].hash == 0) {
                    continue;
                }

                if (moves[i].col == bestMove) {
                    bestHash = moves[i].hash;
                    continue;
                }

#if USE_SMART_GC
                if ((*moves[i].move)->decRefCount() == 0) {
                    auto added = SearchTree::cal.garbage[depth + 1].insert(moves[i].hash);
                    assert(added.second);
                }
#else
                (*moves[i].move)->decRefCount();
#endif   
            }

            assert(movesToWin == BLACK_LOST || bestHash != 0);
        }

        finishMove(bestHash);
    }
    else {
        assert(ET);
    }
}

RedMoveManager::RedMoveManager(const int depth, const Board &curBoard, RedMovePtr* currentMovePtr)
    : MoveManager<RedMovePtr>(depth, curBoard, currentMovePtr, 0) {}

inline void RedMoveManager::finishMove()
{
    ettgcassert(SearchTree::cal.moveCacheLocks[depth].owns_lock());
    {
        if (movesToWin == BLACK_LOST) {
            int oldRefCount = (*currentMovePtr)->getRefCount();
            RedMoveData::setBlackLost(currentMovePtr);
            assert(SearchTree::cal.moveCache[depth].at(currentHash) == *currentMovePtr && *currentMovePtr != nullptr);
            assert((*currentMovePtr)->getRefCount() == oldRefCount);
        }
        else {
            copyHashesToMove();
            (*currentMovePtr)->finish(movesToWin);
        }
    }

    et(SearchTree::cal.moveCVs[depth].notify_all());
}

inline void RedMoveManager::copyHashesToMove()
{
    ettgcassert(SearchTree::cal.moveCacheLocks[depth].owns_lock());
    for (int i = 0; i < BOARD_WIDTH; ++i) {
        if (moves[i].hash == 0) {
            continue;
        }

        (*currentMovePtr)->setNextHash(moves[i].col, moves[i].hash);
    }
}

RedMoveManager::~RedMoveManager() {
    // if move was canceled midway through by another thread, don't do anything
    if (movesToWin != -1) {
        if (movesToWin == BLACK_LOST) {
            ettgc(lGuard(SearchTree::cal.moveCacheLocks[depth + 1]));
            sgc(ettgc(lGuard(SearchTree::cal.garbageLocks[depth + 1])));

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                if (moves[i].hash == 0) {
                    continue;
                }

#if USE_SMART_GC
                if ((*moves[i].move)->decRefCount() == 0) {
                    auto added = SearchTree::cal.garbage[depth + 1].insert(moves[i].hash);
                    assert(added.second);
                }
#else // USE_SMART_GC
                (*moves[i].move)->decRefCount();
#endif // USE_SMART_GC
            }
        }

        finishMove();
    }
    else {
        assert(ET);
        ettgc(lGuard(SearchTree::cal.moveCacheLocks[depth]));
        copyHashesToMove();
    }
}
