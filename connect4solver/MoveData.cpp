#include <cassert>
#include "MoveData.h"
#include "PopCount.h"

using namespace std;

namespace connect4solver {

#define assertMax(var, max) assert((var) / ((max) + 1) == 0)

    typedef unique_ptr<BoardHash[]> next_hash_ptr;

#pragma region m_data Masks

    const int MOVES_TO_WIN_MASK_INDEX = 0;
    const int REF_COUNT_MASK_INDEX = 6;
    const int THREAD_ID_MASK_INDEX = 11;
    const int IS_FINISHED_MASK_INDEX = 14;
    const int IS_SYMMETRIC_MASK_INDEX = 15;

    const ushort MOVES_TO_WIN_MASK = 0x003F << MOVES_TO_WIN_MASK_INDEX; // 0000 0000 0011 1111
    const ushort REF_COUNT_MASK = 0x001F << REF_COUNT_MASK_INDEX;       // 0000 0111 1100 0000
    const ushort THREAD_ID_MASK = 0x0007 << THREAD_ID_MASK_INDEX;       // 0011 1000 0000 0000
    const ushort IS_FINISHED_MASK = 0x0001 << IS_FINISHED_MASK_INDEX;   // 0100 0000 0000 0000
    const ushort IS_SYMMETRIC_MASK = 0x0001 << IS_SYMMETRIC_MASK_INDEX; // 1000 0000 0000 0000

    const ushort REF_COUNT_1 = 0x0040;                                  // 0000 0000 0100 0000
    const ushort FINISH_MASK =                                          // 0111 1000 0011 1111
        IS_FINISHED_MASK |
        THREAD_ID_MASK |
        MOVES_TO_WIN_MASK;

#pragma endregion

#pragma region MoveData

    MoveData::MoveData() : m_data(static_cast<ushort>(-1)) {}

    MoveData::MoveData(const bool isSymmetric) {
        m_data = (isSymmetric * IS_SYMMETRIC_MASK) | THREAD_ID_MASK | REF_COUNT_1;
    }

    const bool MoveData::acquire(const int threadId)
    {
        assertMax(threadId, 7);
        ushort emptyThreadId = m_data | THREAD_ID_MASK;
        ushort withThreadId = m_data ^ ((static_cast<ushort>(-threadId) ^ m_data) & THREAD_ID_MASK);
        return m_data.compare_exchange_strong(emptyThreadId, withThreadId, memory_order::memory_order_acq_rel);
    }

    void MoveData::releaseWithoutFinish()
    {
        assert((m_data & THREAD_ID_MASK) != THREAD_ID_MASK);
        assert(!(m_data & IS_FINISHED_MASK));
        m_data |= THREAD_ID_MASK;
    }

    //void MoveData::setIsFinished(const bool isFinished)
    //{
    //    assert(getIsFinished() == false);
    //    m_data.fetch_xor((-static_cast<ushort>(isFinished) ^ m_data) & IS_FINISHED_MASK);
    //}

    void MoveData::finish(int movesTilWin)
    {
        assertMax(movesTilWin, BLACK_LOST);
        assert(getIsFinished() == false);
        assert(getWorkerThread() != (THREAD_ID_MASK >> THREAD_ID_MASK_INDEX));
        const ushort vals = IS_FINISHED_MASK | THREAD_ID_MASK | (movesTilWin & MOVES_TO_WIN_MASK);
        m_data ^= (FINISH_MASK ^ m_data) & vals;
    }

    const ushort MoveData::incRefCount() {
        assert((m_data & REF_COUNT_MASK) != REF_COUNT_MASK);
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
        return (m_data & REF_COUNT_MASK) >> REF_COUNT_MASK_INDEX;
    }

    const bool MoveData::getIsFinished() const
    {
        return (m_data & IS_FINISHED_MASK) != 0;
    }

    const bool MoveData::getIsSymmetric() const
    {
        return (m_data & IS_SYMMETRIC_MASK) != 0;
    }

    const int MoveData::getWorkerThread() const
    {
        return (m_data & THREAD_ID_MASK) >> THREAD_ID_MASK_INDEX;
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

#pragma endregion

#pragma region RedMoveData

    const uchar IS_CONSENSED_MASK = 0x80;

    RedMoveData::RedMoveData() : MoveData(), m_next(nullptr), m_hashesUsed(0x0) {}

    RedMoveData::RedMoveData(const bool isSymmetric) : MoveData(isSymmetric) {
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
    }

    void RedMoveData::setNextHash(const uint x, const BoardHash hash) {
        assertMax(x, (m_data & IS_SYMMETRIC_MASK) ? MAX_SYMMETRIC_COLUMN : MAX_COLUMN);
        assert(hash != 0);

        m_next[x] = hash;
        m_hashesUsed |= 1 << x;
    }

    const RedHashes RedMoveData::getNextHashes() const {
        if (m_hashesUsed & IS_CONSENSED_MASK) {
            return RedHashes(m_hashesUsed, m_next.get());
        }
        else if (m_data & IS_SYMMETRIC_MASK) {
            return RedHashes(m_hashesUsed, m_next.get(), SYMMETRIC_BOARD_WIDTH);
        }
        else {
            return RedHashes(m_hashesUsed, m_next.get(), BOARD_WIDTH);
        }
    }

    const int RedMoveData::condense()
    {
        assert(m_data & IS_FINISHED_MASK);
        if (m_hashesUsed & IS_CONSENSED_MASK) {
            return 0;
        }

        uchar hashesUsed = m_hashesUsed;
        m_hashesUsed |= IS_CONSENSED_MASK;

        int numHashes = popCountByte(hashesUsed);
        BoardHash* newHashes = new BoardHash[numHashes];

        int hashIdx = 0;
        for (int i = 0; hashesUsed > 0; ++i, hashesUsed >>= 1) {
            if (hashesUsed & 1) {
                newHashes[hashIdx] = m_next[i];
                ++hashIdx;
            }
        }

        m_next.reset();
        m_next = NextHashArrayPtr(newHashes);
        return ((m_data & IS_SYMMETRIC_MASK) ? SYMMETRIC_BOARD_WIDTH : BOARD_WIDTH) - numHashes;
    }

#pragma endregion
}