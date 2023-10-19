#pragma once

#include "types.h"

[[noreturn]] void panic_impl(const char* file, uint line, const char* format, ...);

#define PANIC(...) panic_impl(__FILE__, __LINE__, __VA_ARGS__)
#define ASSERT(cond)                                                                               \
    ((cond) || (panic_impl(__FILE__, __LINE__, "'%s' assert failed", #cond), false))

#define UNREACHABLE() PANIC("Unreachable program state in %s!", __func__)

#define NOT_YET_IMPLEMENTED() PANIC("%s() is not yet implemented", __func__)
