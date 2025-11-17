#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/config.h"

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

typedef struct {
    log_level_t level;
    char tag[12];
    char msg[LOG_BUFFER_LEN];
} log_record_t;

// Global Log queue handle
extern QueueHandle_t g_log_queue;

// Initialize Logging system (create queue)
void logging_init(void);

// Non blocking Logging API used by all modules
void log_post(log_level_t level, const char *tag, const char *fmt, ...);

#endif
