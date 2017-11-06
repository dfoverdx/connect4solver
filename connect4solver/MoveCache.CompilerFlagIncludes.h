#pragma once
#include "CompilerFlags.h"

#if MAP
#include <map>
#define cacheMap std::map
#else
#include <unordered_map>
#define cacheMap std::unordered_map
#endif // MAP

#if ENABLE_THREADING
#include <mutex>
#include <condition_variable>
#endif // ENABLE_THREADING
