#pragma once
#include <cassert>

#define uniqueLineInner2(x, y) x ## y
#define uniqueLineInner1(x, y) uniqueLineInner2(x, y)
#define uniqueLine(x) uniqueLineInner1(x, __LINE__)
#define lGuard(m) std::lock_guard<std::mutex> uniqueLine(lGuard)(m)

#define assertMax(var, max) assert((var) / ((max) + 1) == 0)

#define DllExport __declspec(dllexport)
#define assignConst(type, member, other) *const_cast<type*>(&member) = other.member

#define static_fail(msg) static_fail(msg)

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