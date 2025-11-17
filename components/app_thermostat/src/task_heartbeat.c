#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/config.h"
#include "core/logging.h"

static void task_heartbeat(void *arg) {
    int counter = 0;
    (void)arg;

    while (1) {
        log_post(LOG_LEVEL_INFO, "HEARTBEAT",
                 "System alive, counter=%d", counter++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void task_heartbeat_start(void) {
    xTaskCreate(
        task_heartbeat,
        "task_heartbeat",
        TASK_STACK_HEARTBEAT,
        NULL,
        TASK_PRIO_HEARTBEAT,
        NULL
    );
}
