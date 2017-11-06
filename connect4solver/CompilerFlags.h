#pragma once

/// flags ///

#define MAP                         0
#define ENABLE_THREADING            0
#define VERBOSE                     0
#define USE_SMART_POINTERS          0
#define PACK_MOVE_DATA              0
#define USE_BIT_FIELDS              0
#define USE_PERCENTAGE_MEMORY       0
#define DEPTH_FIRST_SEARCH          1
#define DEBUG_THREADING             0
#define RECREATE_CACHE_AFTER_GC     0

/// TODO? ///

#define USE_THREADED_GC             0

/// negatives ///

#define QUEUE_SEARCH !DEPTH_FIRST_SEARCH

/// flag aliases ///

#define ET ENABLE_THREADING
#define BF USE_BIT_FIELDS
#define TGC USE_THREADED_GC
#define DFS DEPTH_FIRST_SEARCH
#define QS QUEUE_SEARCH

#ifdef NDEBUG
#define DBG                         0
#else // NDEBUG
#define DBG                         1
#endif // NDEBUG