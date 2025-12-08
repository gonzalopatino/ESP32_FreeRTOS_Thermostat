#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include <stdio.h>

// Log level strings with consistent width for alignment
static const char *LEVEL_STR[] = { "DBG", "INF", "WRN", "ERR" };

// ANSI color codes for terminal (optional - can be disabled)
#define LOG_USE_COLORS 1

#if LOG_USE_COLORS
static const char *LEVEL_COLOR[] = {
    "\033[36m",   // DBG: Cyan
    "\033[32m",   // INF: Green
    "\033[33m",   // WRN: Yellow
    "\033[31m"    // ERR: Red
};
#define COLOR_RESET "\033[0m"
#define COLOR_TAG   "\033[94m"  // Light blue for tag
#else
static const char *LEVEL_COLOR[] = { "", "", "", "" };
#define COLOR_RESET ""
#define COLOR_TAG   ""
#endif

static void task_logger(void *arg) {
    (void)arg;

    watchdog_register_current("LOGGER");

    log_record_t rec;

    while (1) {
        if (xQueueReceive(g_log_queue, &rec,
                          pdMS_TO_TICKS(PERIOD_LOGGER_MS)) == pdTRUE) {
            // Clean, aligned log format:
            // [LVL] TAG          | Message
            printf("%s[%s]%s %s%-12s%s| %s\n",
                   LEVEL_COLOR[rec.level],
                   LEVEL_STR[rec.level],
                   COLOR_RESET,
                   COLOR_TAG,
                   rec.tag,
                   COLOR_RESET,
                   rec.msg);
        }
        watchdog_feed();
    }
}

void task_logger_start(void) {
    xTaskCreate(
        task_logger,
        "task_logger",
        TASK_STACK_LOGGER,
        NULL,
        TASK_PRIO_LOGGER,
        NULL
    );
}
