#pragma once
#include "CompilerFlags.h"
#include "Typedefs.h"

namespace connect4solver {
    typedef ushort MoveDataBase;

#if BF
#include <bitfield.h>
    BEGIN_BITFIELD_TYPE(MoveDataInternal, MoveDataBase)
        MoveDataInternal(const MoveDataBase movesToWin, const bool isSymmetric, const bool isFinished, const MoveDataBase refCount, const MoveDataBase workerThreadId) {
        this->movesToWin = movesToWin;
        this->isSymmetric = isSymmetric;
        this->isFinished = isFinished;
        this->refCount = refCount;
        this->workerThreadId = workerThreadId;
    }
    ADD_BITFIELD_MEMBER(movesToWin, connect4solver::moveDataConstants::MOVES_TO_WIN_MASK_INDEX, connect4solver::moveDataConstants::MOVES_TO_WIN_MASK_BITS)
        ADD_BITFIELD_MEMBER(isSymmetric, connect4solver::moveDataConstants::IS_SYMMETRIC_MASK_INDEX, connect4solver::moveDataConstants::IS_SYMMETRIC_MASK_BITS)
        ADD_BITFIELD_MEMBER(isFinished, connect4solver::moveDataConstants::IS_FINISHED_MASK_INDEX, connect4solver::moveDataConstants::IS_FINISHED_MASK_BITS)
        ADD_BITFIELD_MEMBER(refCount, connect4solver::moveDataConstants::REF_COUNT_MASK_INDEX, connect4solver::moveDataConstants::REF_COUNT_MASK_BITS)
        ADD_BITFIELD_MEMBER(workerThreadId, connect4solver::moveDataConstants::WORKER_THREAD_ID_MASK_INDEX, connect4solver::moveDataConstants::WORKER_THREAD_ID_MASK_BITS)
        END_BITFIELD_TYPE()
#else // BF
#if ET
    typedef atomic<MoveDataBase> MoveDataInternal;
#else // ET
    typedef MoveDataBase MoveDataInternal;
#endif // ET
#endif // BF

    class MoveData;
    class BlackMoveData;
    class RedMoveData;

#if USE_SMART_POINTERS
    typedef std::shared_ptr<MoveData> MovePtr;
    typedef std::shared_ptr<BlackMoveData> BlackMovePtr;
    typedef std::shared_ptr<RedMoveData> RedMovePtr;
#else // USE_SMART_POINTERS
    typedef MoveData* MovePtr;
    typedef BlackMoveData* BlackMovePtr;
    typedef RedMoveData* RedMovePtr;
#endif // USE_SMART_POINTERS
}