#pragma once
#include "BoardHash.h"
#include "Piece.h"
#include "Typedefs.h"

namespace connect4solver {
    struct DllExport BoardState {
        static const BoardState fromHash(BoardHash hash);
        static const BoardState fromHash(BoardHash hash, piece &playerTurn);

        ~BoardState() {}

        BoardState& operator=(const BoardState &other);

        const union {
            const BoardPieces pieces;
            const BoardPiecesBitField piecesBF;
        };

        const union {
            const BoardPieces openSpaces;
            const BoardPiecesBitField openSpacesBF;
        };
    };
}