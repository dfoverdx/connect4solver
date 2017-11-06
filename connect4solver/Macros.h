#pragma once
#include <cassert>
#include "CompilerFlags.h"

#if USE_SM
#include <memory>
#include <type_traits>
#endif // USE_SM


#define uniqueLineInner2(x, y) x ## y
#define uniqueLineInner1(x, y) uniqueLineInner2(x, y)
#define uniqueLine(x) uniqueLineInner1(x, __LINE__)
#define ulGuard(m) lGuard uniqueLine(ulGuard)(m)

#define assertMax(var, max) assert((var) / ((max) + 1) == 0)

#define DllExport __declspec(dllexport)
#define assignConst(type, member, other) *const_cast<type*>(&member) = other.member

#define static_fail(msg) static_assert(false, msg)
#define static_fail_undefined_template static_fail("Attempted to call " __FUNCTION__ " in generic template function.")

// macros for giving tests access to the classes they test
#ifdef MS_CPP_UNITTESTFRAMEWORK
#define unitTestClass(testClass) \
namespace connect4solverTests { \
    class testClass; \
}

#define unitTestFriend(testClass) friend class connect4solverTests::testClass
#else // MS_CPP_UNITTESTFRAMEWORK
#define unitTestClass(testClass)
#define unitTestFriend(testClass)
#endif // MS_CPP_UNITTESTFRAMEWORK

// debug value
#if DBG
#define dbgVal(debugValue, releaseValue) debugValue
#define dbg(x) x
#else // DBG
#define dbgVal(debugValue, releaseValue) releaseValue
#define dbg(x) (void(0))
#endif // DBG

#if ENABLE_THREADING
#define et(x) /* enable threading */ x
#define etassert(a) assert(a)
#else // ENABLE_THREADING
#define et(x) /* enable threading */ (void(0))
#define etassert(a) (void(0))
#endif

#if ENABLE_THREADING | USE_THREADED_GC
#define ettgc(x) /* enable threading or use threaded gc */ x
#define ettgcassert(x) assert(x)
#else // ENABLE_THREADING | USE_THREADED_GC
#define ettgc(x) /* enable threading or use threaded gc */ (void(0))
#define ettgcassert(x) (void(0))
#endif // ENABLE_THREADING | USE_THREADED_GC

#if USE_SMART_POINTERS
// reinterpret pointer cast
#define rpc(type, movePtr) reinterpret_cast<type>(movePtr)
// dynamic pointer cast
#define dpc(type, movePtr) std::dynamic_pointer_cast<std::remove_pointer<type>::type>(movePtr)
// static pointer cast
#define spc(type, movePtr) std::static_pointer_cast<std::remove_pointer<type>::type>(movePtr)
#else // USE_SMART_POINTERS
// reinterpret pointer cast
#define rpc(type, movePtr) ((type)(movePtr))
// dynamic pointer cast
#define dpc(type, movePtr) ((type)(movePtr))
// static pointer cast
#define spc(type, movePtr) ((type)(movePtr))
#endif // USE_SMART_POINTERS

// move move pointer casts
#define rspc(movePtr) spc(RedMovePtr, movePtr)
#define bspc(movePtr) spc(BlackMovePtr, movePtr)
#define mspc(movePtr) spc(MovePtr, movePtr)