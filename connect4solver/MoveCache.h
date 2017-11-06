#pragma once
#include "CompilerFlags.h"
#include "Constants.h"
#include "MoveCache.CompilerFlagIncludes.h"
#include "MoveData.h"
#include "Typedefs.h"

namespace connect4solver {
    namespace moveCache {
        typedef cacheMap<BoardHash, MovePtr> Cache;

        extern Cache* moveCaches[MAX_MOVE_DEPTH];

#if ET | TGC
        extern std::mutex moveCacheMutexes[MAX_MOVE_DEPTH];
#endif // ET | TGC

#if ET
        extern std::condition_variable moveCVs[MAX_MOVE_DEPTH];
#endif // ET

        extern void initMoveCache();

#if RECREATE_CACHE_AFTER_GC
#if ET | TGC
        extern void recreateCache(const int depth, const uLock &cacheLock);
        extern void recreateCache(const int depth, const lGuard &cacheLock);
#else // ET | TGC
        extern void recreateCache(const int depth);
#endif // ET | TGC
#endif // RECREATE_CACHE_AFTER_GC
    }
}
