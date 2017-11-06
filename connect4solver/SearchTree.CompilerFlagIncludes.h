#pragma once
#include "CompilerFlags.h"

#if ENABLE_THREADING
#include <bitset>
#include <future>
#endif // ENABLE_THREADING

#if USE_SMART_POINTERS
#include <memory>
#endif // USE_SMART_POINTERS