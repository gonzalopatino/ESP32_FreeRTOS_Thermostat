#ifndef LOGGING_H
#define LOGGING_H

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

// For now there is no logger task, just printf.
// Later we will redirect to the logger task we already discussed.
void logging_init(void);
void log_post(log_level_t level, const char *tag, const char *fmt, ...);

#endif
