#include "CompilerFlags.h"

#if BF
#include <cassert>
#include "Helpers.h"
#include "Macros.h"
#include "PopCount.h"
#include "MoveData.h"

#if ENABLE_THREADING
#define etassert(a) assert(a)
#else
#define etassert(a) (void(0))
#endif

using namespace std;

namespace connect4solver {
    using namespace moveDataConstants;

#pragma region MoveData

    MoveData::MoveData() : m_data(0xFFFF) {}

#if ENABLE_THREADING
    MoveData::MoveData(const MoveData &md) : m_data(md.m_data.load()) {}
#else
    MoveData::MoveData(const MoveData &md) : m_data(md.m_data) {}
#endif

    MoveData::MoveData(const ushort data) : m_data(data) {}

#if BF
    MoveData::MoveData(const bool isSymmetric) {
        m_data.movesToWin = 0;
        m_data.isFinished = isSymmetric;
        m_data.isFinished = false;
        m_data.refCount = 1;
        m_data.workerThreadId = 0xF;
    }

    const MoveData MoveData::createBlackLost(MoveDataInternal data)
    {
        assert(!data.isFinished);
        etassert(data.workerThreadId != 0xF);
        data.isFinished = true;
        data.movesToWin = BLACK_LOST;
        data.workerThreadId = 0xF;
        return MoveData(data);
    }

#else // BF
#define getMaskValue(data, maskName) ((data) & maskName##_MASK) >> maskName##_MASK_INDEX

    inline const bool testFlag(const MoveDataInternal data, const MoveDataInternal mask) {
        return (data & mask) != 0;
    }

    inline const bool MoveData::maskEquals(const MoveDataInternal data, const MoveDataInternal mask)
    {
#ifdef NDEBUG
        return (data & mask) == mask;
#else
        MoveDataInternal val = data & mask;
        return val == mask;
#endif
    }

    inline const bool MoveData::maskEquals(const MoveDataInternal mask) const
    {
        return maskEquals(m_data, mask);
    }

    MoveData::MoveData(const bool isSymmetric) :
        m_data((isSymmetric * IS_SYMMETRIC_MASK) | THREAD_ID_MASK | REF_COUNT_1) {}

    const MoveData MoveData::createBlackLost(MoveDataInternal data)
    {
        assert(!maskEquals(data, IS_FINISHED_MASK));
        etassert(!maskEquals(data, THREAD_ID_MASK));
        return MoveData(data ^ ((data ^ BLACK_LOST_MASK) & BLACK_LOST_MASK));
    }
#endif

#if ENABLE_THREADING

    const bool MoveData::acquire(const int threadId)
    {
        assertMax(threadId, 7);
        MoveDataInternal emptyThreadId = m_data | THREAD_ID_MASK;
        MoveDataInternal withThreadId = (m_data & ~THREAD_ID_MASK) | (threadId << THREAD_ID_MASK_INDEX);
        return m_data.compare_exchange_strong(emptyThreadId, withThreadId, memory_order::memory_order_acq_rel);
    }

    void MoveData::releaseWithoutFinish()
    {
        assert(!mEqualsMask(THREAD_ID_MASK));
        assert(!mEqualsMask(IS_FINISHED_MASK));
        m_data |= THREAD_ID_MASK;
    }
#endif

#if BF
    void MoveData::finish(int movesTilWin)
    {
        assertMax(movesTilWin, MAX_MOVE_DEPTH); // if movesTilWin == BLACK_LOST, use setBlackLost()
        //assert(!maskEquals(IS_FINISHED_MASK));
        assert(!m_data.isFinished);

#if ENABLE_THREADING

        assert(!mEqualsMask(THREAD_ID_MASK));

        MoveDataInternal prev = m_data;
        MoveDataInternal vals = (IS_FINISHED_MASK | THREAD_ID_MASK | (movesTilWin & MOVES_TO_WIN_MASK));
        vals = prev ^ (FINISH_MASK ^ prev) & vals;

        if (!m_data.compare_exchange_strong(prev, vals)) {
            throw exception("Failed to finish");
        }
#else
        //m_data ^= (FINISH_MASK ^ m_data) & (IS_FINISHED_MASK | (movesTilWin & MOVES_TO_WIN_MASK));

        m_data.isFinished = true;
        m_data.movesToWin = movesTilWin;
#endif
    }
#else // BF
    void MoveData::finish(int movesTilWin)
    {
        assertMax(movesTilWin, MAX_MOVE_DEPTH); // if movesTilWin == BLACK_LOST, use setBlackLost()
                                                //assert(!maskEquals(IS_FINISHED_MASK));
        assert(!m_data.isFinished);

#if ENABLE_THREADING

        assert(!mEqualsMask(THREAD_ID_MASK));

        MoveDataInternal prev = m_data;
        MoveDataInternal vals = (IS_FINISHED_MASK | THREAD_ID_MASK | (movesTilWin & MOVES_TO_WIN_MASK));
        vals = prev ^ (FINISH_MASK ^ prev) & vals;

        if (!m_data.compare_exchange_strong(prev, vals)) {
            throw exception("Failed to finish");
        }
#else
        //m_data ^= (FINISH_MASK ^ m_data) & (IS_FINISHED_MASK | (movesTilWin & MOVES_TO_WIN_MASK));

        m_data.isFinished = true;
        m_data.movesToWin = movesTilWin;
#endif
    }
#endif

    const ushort MoveData::incRefCount() {
        //assert(!maskEquals(REF_COUNT_MASK));
        //return ((m_data += REF_COUNT_1) & REF_COUNT_MASK) >> REF_COUNT_MASK_INDEX;

        assert(m_data.refCount < BOARD_WIDTH);
        return ++m_data.refCount;
    }

    const ushort MoveData::decRefCount() {
        /*assert(m_data & REF_COUNT_MASK);
        return ((m_data -= REF_COUNT_1) & REF_COUNT_MASK) >> REF_COUNT_MASK_INDEX;*/

        assert(m_data.refCount > 0);
        return --m_data.refCount;
    }

    const int MoveData::getMovesToWin() const {
        //return m_data & MOVES_TO_WIN_MASK;
        return m_data.movesToWin;
    }

    const int MoveData::getRefCount() const {
        //return getMaskValue(m_data, REF_COUNT);
        return m_data.refCount;
    }

    const bool MoveData::getIsFinished() const
    {
        //return testFlag(m_data, IS_FINISHED_MASK);
        return m_data.isFinished;
    }

    const bool MoveData::getIsSymmetric() const
    {
        //return testFlag(m_data, IS_SYMMETRIC_MASK);
        return m_data.isSymmetric;
    }

#if ENABLE_THREADING
    const int MoveData::getWorkerThread() const
    {
        return getDataValue(THREAD_ID);
    }
#endif

    const bool MoveData::getBlackLost() const {
        //return getMaskValue(m_data, MOVES_TO_WIN) == BLACK_LOST;
        return m_data.movesToWin == BLACK_LOST;
    }

    void MoveData::setBlackLost(MovePtr** mptr) {
#if USE_SMART_POINTERS
        mptr.reset(new MoveData(createBlackLost(MoveDataInternal(mptr->m_data))));
#else // USE_SMART_POINTERS
        MovePtr tmp = new MoveData(createBlackLost(MoveDataInternal((**mptr)->m_data)));
        delete **mptr;
        **mptr = tmp;
#endif // USE_SMART_POINTERS
    }

#pragma endregion

#pragma region BlackMoveData

    BlackMoveData::BlackMoveData() : MoveData(), m_next(0) {}

    BlackMoveData::BlackMoveData(const bool isSymmetric) : MoveData(isSymmetric), m_next(0) {}

    void BlackMoveData::setNextHash(const BoardHash hash) {
        m_next = hash;
    }

    const BoardHash BlackMoveData::getNextHash() const
    {
        return m_next;
    }

    /*void BlackMoveData::setBlackLost(MovePtr &ptr)
    {
        MoveData::setBlackLost(ptr);
    }*/

#pragma endregion

#pragma region RedMoveData

#if USE_DYNAMIC_SIZE_RED_NEXT
    RedMoveData::RedMoveData() : MoveData(), m_next(nullptr) {}
#else // USE_DYNAMIC_SIZE_RED_NEXT
    RedMoveData::RedMoveData() : MoveData(), m_next() {}
#endif // USE_DYNAMIC_SIZE_RED_NEXT

    RedMoveData::RedMoveData(const bool isSymmetric) : MoveData(isSymmetric), m_next() {
#if USE_DYNAMIC_SIZE_RED_NEXT

        if (isSymmetric) {
            m_next = NextHashArrayPtr(new BoardHash[SYMMETRIC_BOARD_WIDTH]);
            for (int i = 0; i < SYMMETRIC_BOARD_WIDTH; ++i) {
                m_next[i] = 0;
            }
        }
        else {
            m_next = NextHashArrayPtr(new BoardHash[BOARD_WIDTH]);
            for (int i = 0; i < BOARD_WIDTH; ++i) {
                m_next[i] = 0;
            }
        }
#endif
    }

#if !USE_SMART_POINTERS
#if USE_DYNAMIC_SIZE_RED_NEXT
    RedMoveData::~RedMoveData()
    {
        if (m_data.movesToWin != BLACK_LOST) {
            delete[] m_next;
        }
    }
#endif
#endif

    void RedMoveData::setNextHash(const int x, const BoardHash hash) {
        assertMax(x, m_data.isSymmetric ? MAX_SYMMETRIC_COLUMN : MAX_COLUMN);
        //assertMax(x, (m_data & IS_SYMMETRIC_MASK) ? MAX_SYMMETRIC_COLUMN : MAX_COLUMN);
        assert(hash != 0);

        m_next[x] = hash;
    }

    const BoardHash* RedMoveData::getNextHashes(int &size) const
    {
        size = (m_data.isSymmetric) ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;
        //size = (m_data & IS_SYMMETRIC_MASK) ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH;

#if USE_DYNAMIC_SIZE_RED_NEXT & USE_SMART_POINTERS
        return m_next.get();
#else
        return m_next;
#endif
    }

#if USE_DYNAMIC_SIZE_RED_NEXT & !USE_SMART_POINTERS
    void RedMoveData::setBlackLost(RedMovePtr** ptr)
    {
        delete[](**ptr)->m_next;
        MoveData::setBlackLost((MovePtr**)ptr);
    }
#endif

#pragma endregion
}
#endif