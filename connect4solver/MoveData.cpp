#include "MoveCache.h"
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

MoveData* MoveData::getBlackLostMove() const {
    return new MoveData(createBlackLostData(m_data));
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

RedMoveData::RedMoveData() : MoveData(), m_next() {}
RedMoveData::RedMoveData(const bool isSymmetric) : MoveData(isSymmetric), m_next() {}

#pragma endregion

namespace {
    inline void decRedNextRefsInner(RedMovePtr &ptr, const int depth) {
        int size;
        const BoardHash* hashes = ptr->getNextHashes(size);
        moveCache::Cache &cache = *moveCache::moveCaches[depth + 1];

        for (int i = 0; i < size; ++i) {
            if (hashes[i] != 0) {
                cache.at(hashes[i])->decRefCount();
            }
        }
    }
}

#if ET | TGC
void connect4solver::decRedNextRefs(RedMovePtr &ptr, const int depth, const uLock &cacheLock)
{
    ettgcassert(cacheLock.mutex() == &moveCache::moveCacheMutexes[depth + 1] && cacheLock.owns_lock());
    decRedNextRefsInner(ptr, depth);
}

void connect4solver::decRedNextRefs(RedMovePtr &ptr, const int depth, const lock_guard<mutex> &cacheLock)
{
    decRedNextRefsInner(ptr, depth);
}
#else // ET | TGC
void connect4solver::decRedNextRefs(RedMovePtr &ptr, const int depth)
{
    decRedNextRefsInner(ptr, depth);
}
#endif // ET | TGC