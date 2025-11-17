#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include <stdio.h>

static const char *LEVEL_STR[] = { "D", "I", "W", "E" };

static void task_logger(void *arg) {
    (void)arg;

    watchdog_register_current("LOGGER");

    log_record_t rec;

    while (1) {
        if (xQueueReceive(g_log_queue, &rec,
                          pdMS_TO_TICKS(PERIOD_LOGGER_MS)) == pdTRUE) {
            // Structured JSON like log line
            printf("{\"lvl\":\"%s\",\"tag\":\"%s\",\"msg\":\"%s\"}\n",
                   LEVEL_STR[rec.level], rec.tag, rec.msg);
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
