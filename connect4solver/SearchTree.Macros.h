#pragma once
#include "CompilerFlags.h"
#include "Macros.h"
#include "Typedefs.h"

#if USE_SMART_POINTERS
#define usp(x) /* use smart pointers */ x
#define nusp(x) /* !use smart pointers */ (void(0))

#define TDataPtr shared_ptr<TData>

#define rspc(movePtr) spc(RedMoveData, movePtr)
#define bspc(movePtr) spc(BlackMoveData, movePtr)
#define mspc(movePtr) spc(MoveData, movePtr)
#else // USE_SMART_POINTERS
#define usp(x) /* use smart pointers */ (void(0))
#define nusp(x) /* !use smart pointers */ x

#define TDataPtr TData*
#endif // USE_SMART_POINTERS

#if ENABLE_THREADING
#define setProgress(progress) state.stats.threadProgress[threadId] = progress
#define currentMoveRoute allCurrentMoveRoutes[threadId]
#define boards allBoards[threadId]
#else // ENABLE_THREADING
#define setProgress(progress) state.totalProgress = progress
#endif