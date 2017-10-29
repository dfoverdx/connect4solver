#include "CompilerFlags.h"
#if !BF

#include <cassert>
#include "MoveData.h"
#include "Helpers.h"
#include "Macros.h"
#include "MoveData.h"
#include "PopCount.h"

#if ENABLE_THREADING
#define etassert(a) assert(a)
#else
#define etassert(a) (void(0))
#endif

using namespace std;
using namespace connect4solver;
using namespace connect4solver::moveDataConstants;

#define getMaskValue(data, maskName) ((data) & maskName##_MASK) >> maskName##_MASK_INDEX

inline const bool testFlag(const MoveDataBase data, const MoveDataBase mask) {
    return (data & mask) != 0;
}

inline const bool MoveData::maskEquals(const MoveDataBase data, const MoveDataBase mask)
{
#ifdef NDEBUG
    return (data & mask) == mask;
#else
    MoveDataBase val = data & mask;
    return val == mask;
#endif
}

inline const bool MoveData::maskEquals(const MoveDataBase mask) const
{
    return maskEquals(m_data, mask);
}

MoveData::MoveData(const bool isSymmetric) :
    m_data((isSymmetric * IS_SYMMETRIC_MASK) | WORKER_THREAD_ID_MASK | REF_COUNT_1) {}

const MoveData MoveData::createBlackLost(MoveDataBase data)
{
    assert(!maskEquals(data, IS_FINISHED_MASK));
    etassert(!maskEquals(data, WORKER_THREAD_ID_MASK));
    return MoveData(MoveDataBase((data & ~BLACK_LOST_MASK) | BLACK_LOST_MASK));
}

#if ENABLE_THREADING
const bool MoveData::acquire(const int threadId)
{
    assertMax(threadId, 7);
    MoveDataBase emptyThreadId = m_data | WORKER_THREAD_ID_MASK;
    MoveDataBase withThreadId = (emptyThreadId & ~WORKER_THREAD_ID_MASK) | (threadId << WORKER_THREAD_ID_MASK_INDEX);
    return m_data.compare_exchange_strong(emptyThreadId, withThreadId, memory_order::memory_order_acq_rel);
}

void MoveData::releaseWithoutFinish()
{
    assert(!maskEquals(WORKER_THREAD_ID_MASK));
    assert(!maskEquals(IS_FINISHED_MASK));
    m_data |= WORKER_THREAD_ID_MASK;
}
#endif

void MoveData::finish(int movesTilWin)
{
    assertMax(movesTilWin, MAX_MOVE_DEPTH); // if movesTilWin == BLACK_LOST, use setBlackLost()
    assert(!maskEquals(IS_FINISHED_MASK));

    MoveDataBase val = (IS_FINISHED_MASK | WORKER_THREAD_ID_MASK | (movesTilWin & MOVES_TO_WIN_MASK));

#if ENABLE_THREADING
    assert(!maskEquals(WORKER_THREAD_ID_MASK));

    MoveDataBase prev = m_data;

    if (!m_data.compare_exchange_strong(prev, vals)) {
        throw exception("Failed to finish");
    }
#else // ENABLE_THREADING
    m_data = (m_data & ~FINISH_MASK) | val;
#endif // ENABLE_THREADING
}

const ushort MoveData::incRefCount() {
    assert(!maskEquals(REF_COUNT_MASK));
    return ((m_data += REF_COUNT_1) & REF_COUNT_MASK) >> REF_COUNT_MASK_INDEX;
}

const ushort MoveData::decRefCount() {
    assert(m_data & REF_COUNT_MASK);
    return ((m_data -= REF_COUNT_1) & REF_COUNT_MASK) >> REF_COUNT_MASK_INDEX;
}

const int MoveData::getMovesToWin() const {
    return m_data & MOVES_TO_WIN_MASK;
}

const int MoveData::getRefCount() const {
    return getMaskValue(m_data, REF_COUNT);
}

const bool MoveData::getIsFinished() const
{
    return testFlag(m_data, IS_FINISHED_MASK);
}

const bool MoveData::getIsSymmetric() const
{
    return testFlag(m_data, IS_SYMMETRIC_MASK);
}

#if ENABLE_THREADING
const int MoveData::getWorkerThread() const
{
    return getMaskValue(m_data, WORKER_THREAD_ID);
}
#endif // ENABLE_THREADING

const bool MoveData::getBlackLost() const {
    return getMaskValue(m_data, MOVES_TO_WIN) == BLACK_LOST;
}

#pragma endregion

#pragma region RedMoveData
#if USE_DYNAMIC_SIZE_RED_NEXT & !USE_SMART_POINTERS
RedMoveData::~RedMoveData()
{
    if (getMaskValue(m_data, MOVES_TO_WIN) != BLACK_LOST) {
        delete[] m_next;
    }
}
#endif // USE_DYNAMIC_SIZE_RED_NEXT & !USE_SMART_POINTERS

void RedMoveData::setNextHash(const int x, const BoardHash hash) {
    assertMax(x, (m_data & IS_SYMMETRIC_MASK) ? MAX_SYMMETRIC_COLUMN : MAX_COLUMN);
    assert(hash != 0);
    m_next[x] = hash;
}

const BoardHash* RedMoveData::getNextHashes(int &size) const
{
    size = (m_data & IS_SYMMETRIC_MASK) ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;

#if USE_DYNAMIC_SIZE_RED_NEXT & USE_SMART_POINTERS
    return m_next.get();
#else // USE_DYNAMIC_SIZE_RED_NEXT & USE_SMART_POINTERS
    return m_next;
#endif // USE_DYNAMIC_SIZE_RED_NEXT & USE_SMART_POINTERS
}

#pragma endregion
#endif