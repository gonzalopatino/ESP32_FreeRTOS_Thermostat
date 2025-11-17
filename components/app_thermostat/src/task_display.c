#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"

#include "app/task_common.h"        // g_q_thermostat_state
#include "app/task_display.h"

#include "drivers/drv_display.h"    // drv_display_*
#include "core/thermostat.h"        // thermostat_state_t

static const char *TAG = "DISPLAY";

/**
 * @brief Display task: subscribes to thermostat_state_t and renders it.
 */
static void task_display(void *arg) {
    (void)arg;

    watchdog_register_current("DISPLAY");
    drv_display_init();

    thermostat_state_t state;

    while (1) {
        if (xQueueReceive(g_q_thermostat_state, &state, portMAX_DELAY) == pdTRUE) {
            drv_display_show_state(&state);
        }
        watchdog_feed();
    }
}

void task_display_start(void) {
    xTaskCreate(
        task_display,
        "task_display",
        TASK_STACK_DISPLAY,   // define in config.h
        NULL,
        TASK_PRIO_DISPLAY,    // define in config.h
        NULL
    );
}
