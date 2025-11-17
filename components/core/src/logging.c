#include "core/logging.h"
#include <stdio.h>
#include <stdarg.h>

void logging_init(void) {
    // Later: initialize log queue, logger task, etc.
}

void log_post(log_level_t level, const char *tag, const char *fmt, ...) {
    (void)level; // not used yet

    va_list args;
    va_start(args, fmt);

    printf("[%s] ", tag);
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}
