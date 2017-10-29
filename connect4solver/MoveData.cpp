#include "MoveData.h"

using namespace std;
using namespace connect4solver;

#pragma region MoveData

MoveData::MoveData() : m_data(0xFFFF) {}

MoveData::MoveData(const MoveDataBase data) : m_data(data) {}

#if ENABLE_THREADING
MoveData::MoveData(const MoveData &md) : m_data(md.m_data.load()) {}
#else
MoveData::MoveData(const MoveData &md) : m_data(md.m_data) {}
#endif

void MoveData::setBlackLost(MovePtr* mptr) {
#if USE_SMART_POINTERS
    mptr.reset(new MoveData(createBlackLost(MoveDataInternal(mptr->m_data))));
#else // USE_SMART_POINTERS
    MovePtr tmp = new MoveData(createBlackLost(MoveDataInternal((*mptr)->m_data)));
    delete *mptr;
    *mptr = tmp;
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

#pragma endregion

#pragma region RedMoveData

#if USE_DYNAMIC_SIZE_RED_NEXT
RedMoveData::RedMoveData() : MoveData(), m_next(nullptr) {}

RedMoveData::RedMoveData(const bool isSymmetric) : MoveData(isSymmetric), m_next() {
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
#else // USE_DYNAMIC_SIZE_RED_NEXT
RedMoveData::RedMoveData() : MoveData(), m_next() {}
RedMoveData::RedMoveData(const bool isSymmetric) : MoveData(isSymmetric), m_next() {}
#endif // USE_DYNAMIC_SIZE_RED_NEXT


void RedMoveData::setBlackLost(RedMovePtr* ptr)
{
#if USE_DYNAMIC_SIZE_RED_NEXT & !USE_SMART_POINTERS
    delete[](*ptr)->m_next;
#endif // USE_DYNAMIC_SIZE_RED_NEXT & !USE_SMART_POINTERS
    MoveData::setBlackLost((MovePtr*)ptr);
}

#pragma endregion