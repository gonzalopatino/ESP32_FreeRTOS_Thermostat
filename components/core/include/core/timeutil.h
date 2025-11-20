#pragma once

#include <stdbool.h>
#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start SNTP client
void timeutil_init_sntp(void);

// Returns true once SNTP has set the clock
bool timeutil_is_time_set(void);

// Write current local time into buf as ISO8601 format
// Example: "2025-11-20T06:32:47-08:00"
// buf_len must be >= 32
bool timeutil_get_iso8601(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
