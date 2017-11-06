#pragma once
#include <mutex>
#include "CompilerFlags.h"
#include "Macros.h"

typedef char tinyint;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned long ulong;
typedef unsigned long long ull;

typedef std::unique_lock<std::mutex> uLock;
typedef std::lock_guard<std::mutex> lGuard;

namespace connect4solver {
    typedef DllExport int Heuristic;
    typedef DllExport unsigned long long BoardPieces;
    typedef DllExport unsigned long long BoardHash;
}