#include <cassert>
#include "Macros.h"
#include "MoveCache.h"

using namespace std;
using namespace connect4solver;
using namespace connect4solver::moveCache;

namespace {
    bool initialized = false;

#if RECREATE_CACHE_AFTER_GC
    inline void recreateCacheInner(const int depth);
#endif // RECREATE_CACHE_AFTER_GC 
}

Cache* connect4solver::moveCache::moveCaches[MAX_MOVE_DEPTH];

#if ET | TGC
std::mutex connect4solver::moveCache::moveCacheMutexes[MAX_MOVE_DEPTH];
#endif // ET | TGC

#if ET
std::condition_variable connect4solver::moveCache::moveCVs[MAX_MOVE_DEPTH];
#endif // ET

void connect4solver::moveCache::initMoveCache()
{
    assert(!initialized);

    for (int i = 0; i < MAX_MOVE_DEPTH; ++i) {
        moveCaches[i] = new Cache();
    }

    initialized = true;
}

#if RECREATE_CACHE_AFTER_GC
#if ET | TGC
void connect4solver::moveCache::recreateCache(const int depth, const uLock &cacheLock)
{
    ettgcassert(cacheLock.mutex() == &moveCacheMutexes[depth] && cacheLock.owns_lock());
    recreateCacheInner(depth);
}

void connect4solver::moveCache::recreateCache(const int depth, const lGuard &cacheLock)
{
    recreateCacheInner(depth);
}
#else // ET | TGC
void connect4solver::moveCache::recreateCache(const int depth)
{
    recreateCacheInner(depth);
}
#endif // ET | TGC

namespace {
    inline void recreateCacheInner(const int depth) {
        Cache* tmp = new Cache(moveCaches[depth]->begin(), moveCaches[depth]->end());
        delete moveCaches[depth];
        moveCaches[depth] = tmp;
    }
}
#endif // RECREATE_CACHE_AFTER_GC