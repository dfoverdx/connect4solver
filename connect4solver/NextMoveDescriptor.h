#pragma once
#include "Macros.h"
#include "MoveData.CompilerFlagTypedefs.h"
#include "Typedefs.h"

namespace connect4solver {
    struct NextMoveDescriptor {
    protected:
        MovePtr m_move = nullptr;

    public:
        NextMoveDescriptor() : col(0), hash(0), heur(0), m_move(nullptr) {}
        NextMoveDescriptor(const int col, const BoardHash hash, const Heuristic heur, const MovePtr move) :
            col(col), hash(hash), heur(heur), m_move(move) {}

        int col = 0;
        BoardHash hash = 0;
        Heuristic heur = 0;

        MovePtr getMove() const {
            return m_move;
        }

        void putMove(MovePtr ptr) {
            m_move = ptr;
        }

        __declspec(property(get=getMove, put=putMove)) MovePtr move;

        friend bool operator <(const NextMoveDescriptor &l, const NextMoveDescriptor &r) {
            return l.heur < r.heur;
        }

        friend bool operator >(const NextMoveDescriptor &l, const NextMoveDescriptor &r) {
            return l.heur > r.heur;
        }

        friend bool operator <=(const NextMoveDescriptor &l, const NextMoveDescriptor &r) {
            return l.heur <= r.heur;
        }

        friend bool operator >=(const NextMoveDescriptor &l, const NextMoveDescriptor &r) {
            return l.heur >= r.heur;
        }
    };

    struct BlackNextMoveDescriptor : public NextMoveDescriptor {
        BlackMovePtr getMove() const {
            return dpc(BlackMovePtr, m_move);
        }

        void putMove(BlackMovePtr ptr) {
            m_move = mspc(ptr);
        }

        __declspec(property(get = getMove, put = putMove)) BlackMovePtr move;
    };

    struct RedNextMoveDescriptor : public NextMoveDescriptor {
        RedMovePtr getMove() const {
            return dpc(RedMovePtr, m_move);
        }

        void putMove(RedMovePtr ptr) {
            m_move = mspc(ptr);
        }

        __declspec(property(get = getMove, put = putMove)) RedMovePtr move;
    };
}