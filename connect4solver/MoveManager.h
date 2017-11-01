#pragma once
#include <array>
#include <cassert>
#include <type_traits>
#include <typeinfo>
#include "Board.h"
#include "Constants.h"
#include "Macros.h"
#include "MoveData.h"
#include "NextMoveDescriptor.h"
#include "SearchTree.h"
#include "SearchTree.Macros.h"

namespace connect4solver {
    template <typename CurMPtr>
    class MoveManager {
        typedef typename std::conditional<std::is_same<CurMPtr, BlackMovePtr>::value, RedMovePtr, BlackMovePtr>::type NextMPtr;
        typedef typename std::remove_pointer<NextMPtr>::type NextMData;

    protected:
        MoveManager(const int depth, const Board &curBoard, CurMPtr* currentMovePtr, int movesToWin) :
            depth(depth), nextDepth(depth + 1), currentHash(curBoard.boardHash), validMoves(curBoard.validMoves), 
            currentMovePtr(currentMovePtr), movesToWin(movesToWin)
        {
            assert(currentMovePtr != nullptr && *currentMovePtr != nullptr);
            assert((depth % 2) ^ (std::is_same<CurMPtr, RedMovePtr>::value));

            for (int i = 0; i < BOARD_WIDTH; ++i) {
                moves[i] = { 0, 0, 0, nullptr };
            }
        }

        virtual inline void finishMove() { throw std::exception("Attempted to call finish move from abstract MoveManager"); };

    public:
        CurMPtr* currentMovePtr;
        const BoardHash currentHash;
        const ValidMoves validMoves;
        const int depth;
        const int nextDepth;
        int movesToWin;
        std::array<NextMoveDescriptor<NextMPtr>, BOARD_WIDTH> moves;

        NextMoveDescriptor<NextMPtr>& operator[](int i)
        {
            return moves[i];
        }

        void addMove(const int i, const Board &nextBoard) {
            assert(nextDepth < MAX_MOVE_DEPTH);
            ettgcassert(SearchTree::cal.moveCacheLocks[nextDepth].owns_lock());
            assert(validMoves.cols[i]);

            moves[i].heur = nextBoard.heuristic;
            
            if (moves[i].move == nullptr) {
                MoveCache &mc = SearchTree::cal.moveCache[nextDepth];
                assert(mc.find(nextBoard.boardHash) == mc.end());
                auto it = mc.insert({ nextBoard.boardHash, NextMPtr(new NextMData(nextBoard.symmetryBF.isSymmetric)) });

                NextMPtr* tmp = rpc(NextMPtr*, &it.first->second);
                moves[i].move = tmp;
            }
            else {
                // happens when we add a move to the cache and then another board short-circuits the search of the
                // unfinished move
                assert(!(*moves[i].move)->getIsFinished());
            }
        }
    };

    class BlackMoveManager : public MoveManager<BlackMovePtr> {
    protected:
        inline void finishMove(const BoardHash bestHash);

    public:
        BlackMoveManager(const int depth, const Board &curBoard, BlackMovePtr* currentMovePtr);
        ~BlackMoveManager();

        int bestMove;
    };

    class RedMoveManager : public MoveManager<RedMovePtr> {
    protected:
        inline void finishMove();
        inline void copyHashesToMove();

    public:
        RedMoveManager(const int depth, const Board &curBoard, RedMovePtr* currentMovePtr);
        ~RedMoveManager();
    };
}
