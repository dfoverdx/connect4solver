#pragma once
#include "Typedefs.h"

namespace connect4solver {
    template<typename NextMPtr>
    struct NextMoveDescriptor {
        int col;
        BoardHash hash;
        Heuristic heur;
        NextMPtr* move;

        friend bool operator <(const NextMoveDescriptor<NextMPtr> &l, const NextMoveDescriptor<NextMPtr> &r) {
            return l.heur < r.heur;
        }

        friend bool operator >(const NextMoveDescriptor<NextMPtr> &l, const NextMoveDescriptor<NextMPtr> &r) {
            return l.heur > r.heur;
        }

        friend bool operator <=(const NextMoveDescriptor<NextMPtr> &l, const NextMoveDescriptor<NextMPtr> &r) {
            return l.heur <= r.heur;
        }

        friend bool operator >=(const NextMoveDescriptor<NextMPtr> &l, const NextMoveDescriptor<NextMPtr> &r) {
            return l.heur >= r.heur;
        }
    };
}