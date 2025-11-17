#include "core/logging.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Global handle to the Logging queue.
// Tasks will push log messages into this queue, and a Logger task
// (running in another component) will pull and process them.
QueueHandle_t g_log_queue = NULL;

void logging_init(void) {
    // Allocate a queue that holds LOG_QUEUE_LENGTH number of Log_record_t entries.
    // If this queue is NULL, Logging falls back to printf.
    g_log_queue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(log_record_t));
}

void log_post(log_level_t level, const char *tag, const char *fmt, ...) {

    // If Logging hasn't been initalized yet, fall back to direct printing
    if (g_log_queue == NULL){
        // As a fallback, we can still print directly
        va_list args;
        va_start(args, fmt);
        
        // Simple synchronous print
        printf("[%s]", tag);
        vprintf(fmt, args);
        printf("/n");

        va_end(args);
        return;
    }

    // Prepare a Log record and zero-fill all fields
    log_record_t rec = {0};
    rec.level = level;

    // Copy tag into the struct safely (bounded copy)
    strncpy(rec.tag, tag, sizeof(rec.tag) - 1);
    rec.tag[sizeof(rec.tag) - 1] = '\0'; // Ensure null termination

    // Extract variable arguments (like printf)
    va_list args;
    va_start(args, fmt);

    // Format the final Log message safely into rec.msg
    vsnprintf(rec.msg, sizeof(rec.msg), fmt, args);

    va_end(args);

    // Push the Log record into the queue
    // Timeout = 0 means "do not block" -- if the queue is full, drop the Log.
    // This avoids blocking a high-priority task just because Logging is busy

    // Do not block forever, drop if queue is full
    xQueueSend(g_log_queue, &rec, 0);
}
