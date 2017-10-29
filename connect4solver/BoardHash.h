#pragma once
#include <bitfield.h>
#include "Constants.h"
#include "Constants.Board.h"
#include "Typedefs.h"

namespace connect4solver {
    namespace {
        constexpr int EMPTY_COL_BIT_COUNT = 8 - BOARD_HEIGHT;
    }

    BEGIN_BITFIELD_TYPE(BoardPiecesBitField, BoardPieces)
        ADD_BITFIELD_ARRAY_WITH_EMPTY_BITS(cols, 0, BOARD_HEIGHT, EMPTY_COL_BIT_COUNT, BOARD_WIDTH)
    END_BITFIELD_TYPE()

    BEGIN_BITFIELD_TYPE(BoardHashBitField, BoardHash)
        ADD_BITFIELD_ARRAY(cols, 0, BOARD_HEIGHT, BOARD_WIDTH)
        ADD_BITFIELD_ARRAY(heights, BOARD_SIZE, COLUMN_HEIGHT_REQUIRED_BITS, BOARD_WIDTH)
    END_BITFIELD_TYPE()
}