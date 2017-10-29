#pragma once
#include "CompilerFlags.h"
#include "Macros.h"
#include "Typedefs.h"

#if USE_SMART_GC
#define sgc(x) /* use smart gc */ x
#else // USE_SMART_GC
#define sgc(x) /* use smart gc */ (void(0))
#endif // USE_SMART_GC

#if USE_SMART_POINTERS
#define usp(x) /* use smart pointers */ x
#define nusp(x) /* !use smart pointers */ (void(0))

#define TDataPtr shared_ptr<TData>

// reinterpret pointer cast
#define rpc(type, movePtr) reinterpret_cast<type>(movePtr)

// static pointer cast
#define spc(dataType, movePtr) static_pointer_cast<dataType>(movePtr)
#define rspc(movePtr) spc(RedMoveData, movePtr)
#define bspc(movePtr) spc(BlackMoveData, movePtr)
#define mspc(movePtr) spc(MoveData, movePtr)
#else // USE_SMART_POINTERS
#define usp(x) /* use smart pointers */ (void(0))
#define nusp(x) /* !use smart pointers */ x

#define TDataPtr TData*

// reinterpret pointer cast
#define rpc(type, movePtr) ((type)(movePtr))

// static pointer cast
#define spc(dataType, movePtr) ((dataType*)(movePtr))
#define rspc(movePtr) ((RedMoveData*)(movePtr))
#define bspc(movePtr) ((BlackMoveData*)(movePtr))
#define mspc(movePtr) ((MoveData*)(movePtr))
#endif // USE_SMART_POINTERS

#if MAP
#define cacheMap std::map
#else // MAP
#define cacheMap std::unordered_map
#endif // MAP

#if ENABLE_THREADING
#define et(x) /* enable threading */ x
#define etassert(a) assert(a)
#define setProgress(progress) status.stats.threadProgress[threadId] = progress
#define currentMoveRoute allCurrentMoveRoutes[threadId]
#define boards allBoards[threadId]
#else // ENABLE_THREADING
#define et(x) /* enable threading */ (void(0))
#define etassert(a) (void(0))
#define setProgress(progress) status.totalProgress = progress
#endif

#if ENABLE_THREADING | USE_THREADED_GC
#define ettgc(x) /* enable threading or use threaded gc */ x
#define ettgcassert(x) assert(x)
#else // ENABLE_THREADING | USE_THREADED_GC
#define ettgc(x) /* enable threading or use threaded gc */ (void(0))
#define ettgcassert(x) (void(0))
#endif // ENABLE_THREADING | USE_THREADED_GC