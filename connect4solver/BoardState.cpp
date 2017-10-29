#include "BoardState.h"
#include "Constants.h"
#include "Constants.Board.h"

namespace connect4solver {
    inline const BoardState getBoardStateFromHash(BoardHashBitField bf) {
        BoardPiecesBitField pbf;
        BoardPiecesBitField osbf;

        for (int i = 0; i < BOARD_WIDTH; ++i) {
            pbf.cols[i] = bf.cols[i];
            osbf.cols[i] = (EMPTY_COLUMN_BITS << bf.heights[i]) & EMPTY_COLUMN_BITS;
        }

        return BoardState{ pbf, osbf };
    }

    const BoardState BoardState::fromHash(BoardHash hash)
    {
        return getBoardStateFromHash(hash);
    }

    const BoardState BoardState::fromHash(BoardHash hash, piece &playerTurn)
    {
        BoardHashBitField bf(hash);
        int filledPieces = 0;
        for (int i = 0; i < BOARD_WIDTH; ++i) {
            filledPieces += static_cast<int>(bf.heights[i]);
        }
        playerTurn = piece(filledPieces % 2);

        return getBoardStateFromHash(bf);
    }

    BoardState& BoardState::operator=(const BoardState & other)
    {
        assignConst(BoardPieces, pieces, other);
        assignConst(BoardPieces, openSpaces, other);
        return *this;
    }

}