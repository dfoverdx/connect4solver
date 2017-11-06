#pragma once
#include "Constants.h"
#include "Helpers.h"
#include "MoveData.CompilerFlagTypedefs.h"

namespace connect4solver {
    namespace moveDataConstants {
        constexpr MoveDataBase MOVES_TO_WIN_MASK_BITS = RequiredBits<BOARD_SIZE>::value;
        constexpr MoveDataBase IS_FINISHED_MASK_BITS = 1;
        constexpr MoveDataBase IS_SYMMETRIC_MASK_BITS = 1;
        constexpr MoveDataBase REF_COUNT_MASK_BITS = RequiredBits<BOARD_WIDTH>::value;
        constexpr MoveDataBase WORKER_THREAD_ID_MASK_BITS = 4;

#define makeIndex(prevMask) prevMask##_MASK_INDEX + prevMask##_MASK_BITS

        constexpr MoveDataBase MOVES_TO_WIN_MASK_INDEX = 0;
        constexpr MoveDataBase IS_SYMMETRIC_MASK_INDEX = makeIndex(MOVES_TO_WIN);
        constexpr MoveDataBase IS_FINISHED_MASK_INDEX = makeIndex(IS_SYMMETRIC);
        constexpr MoveDataBase REF_COUNT_MASK_INDEX = makeIndex(IS_FINISHED);
        constexpr MoveDataBase WORKER_THREAD_ID_MASK_INDEX = makeIndex(REF_COUNT);

#undef makeIndex

#if !BF
#define makeMask(maskName) constexpr MoveDataBase maskName##_MASK = ((1 << maskName##_MASK_BITS) - 1) << maskName##_MASK_INDEX

        makeMask(MOVES_TO_WIN);                                                 // ---- ---- --11 1111
        makeMask(IS_SYMMETRIC);                                                 // ---- ---- -1-- ----
        makeMask(IS_FINISHED);                                                  // ---- ---- 1--- ----
        makeMask(REF_COUNT);                                                    // ---- -111 ---- ----
        makeMask(WORKER_THREAD_ID);                                             // -111 1--- ---- ----

        constexpr MoveDataBase REF_COUNT_1 = 0x1 << REF_COUNT_MASK_INDEX;       // ---- ---1 ---- ----
        constexpr MoveDataBase FINISH_MASK =                                    // -111 1--- 1111 1111
            IS_FINISHED_MASK |
            WORKER_THREAD_ID_MASK |
            MOVES_TO_WIN_MASK;
        constexpr MoveDataBase BLACK_LOST_MASK =                                // -111 1--- 111- 1-1-
            IS_FINISHED_MASK |
            WORKER_THREAD_ID_MASK |
            BLACK_LOST << MOVES_TO_WIN_MASK_INDEX;

#undef makeMask
#endif
    }
}