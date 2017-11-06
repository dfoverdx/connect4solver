#pragma once
#include <array>
#include <cassert>
#include <type_traits>
#include <typeinfo>
#include "Board.h"
#include "Constants.h"
#include "MoveData.h"
#include "NextMoveDescriptor.h"

namespace connect4solver {
    class MoveManager {
    private:
        inline void addMoveInner(const int x, const Board &nextBoard);

    protected:
        typedef std::array<NextMoveDescriptor, BOARD_WIDTH> MoveDescriptorArray;

        MoveManager(const int depth, const BoardHash hash, const ValidMoves validMoves, MovePtr curMovePtr, const int movesToWin) :
            currentHash(hash), validMoves(validMoves), depth(depth), nextDepth(depth + 1), m_curMovePtr(curMovePtr), movesToWin(movesToWin) {
            assert(nextDepth < MAX_MOVE_DEPTH);
            assert(m_curMovePtr != nullptr);
        }

        MovePtr m_curMovePtr;
        MoveDescriptorArray m_moves;

        virtual inline MovePtr makeMovePtr(const bool isSymmetric) = 0;

    public:
        virtual ~MoveManager() {};

        NextMoveDescriptor& operator[](int x);
        
        virtual inline void sortByHeuristics() = 0;

#if ET | TGC
        void addMove(const int x, const Board &nextBoard, const uLock &cacheLock);
        void addMove(const int x, const Board &nextBoard, const lGuard &cacheLock);
#else // ET | TGC
        void addMove(const int x, const Board &nextBoard);
#endif // ET | TGC

        const BoardHash currentHash;
        const ValidMoves validMoves;
        const int depth;
        const int nextDepth;
        int movesToWin = 0;

        NextMoveDescriptor& getMoves(int i) {
            return static_cast<BlackNextMoveDescriptor&>(m_moves[i]);
        }

        void putMoves(int i, NextMoveDescriptor val) {
            m_moves[i] = val;
        }

        __declspec(property(get = getMoves, put = putMoves)) NextMoveDescriptor moves[];
    };

    class BlackMoveManager : public MoveManager {
    private:
        inline MovePtr makeMovePtr(const bool isSymmetric);

    protected:
        inline void finishMove(const BoardHash bestHash);

    public:
        BlackMoveManager(const int depth, const Board &curBoard, BlackMovePtr curMovePtr);
        ~BlackMoveManager();

        int bestMove;

        inline void sortByHeuristics();

        BlackNextMoveDescriptor& getMoves(int i) {
            return static_cast<BlackNextMoveDescriptor&>(m_moves[i]);
        }

        void putMoves(int i, BlackNextMoveDescriptor val) {
            m_moves[i] = val;
        }

        __declspec(property(get = getMoves, put = putMoves)) BlackNextMoveDescriptor moves[];
    };

    class RedMoveManager : public MoveManager {
    private:
        inline MovePtr makeMovePtr(const bool isSymmetric);

    protected:
        inline void finishMove();
        inline void copyHashesToMove();

    public:
        RedMoveManager(const int depth, const Board &curBoard, RedMovePtr curMovePtr);
        ~RedMoveManager();

        inline void sortByHeuristics();

        RedNextMoveDescriptor& getMoves(int i) {
            return static_cast<RedNextMoveDescriptor&>(m_moves[i]);
        }

        void putMoves(int i, RedNextMoveDescriptor val) {
            m_moves[i] = val;
        }

        __declspec(property(get = getMoves, put = putMoves)) RedNextMoveDescriptor moves[];
    };
}
