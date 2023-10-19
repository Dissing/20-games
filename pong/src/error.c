#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

void panic_impl(const char* file, uint line, const char* format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "PANIC: %s:%u\n", file, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    raise(SIGABRT);
    exit(1);
}
