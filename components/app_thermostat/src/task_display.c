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
 *
 * This task:
 *  - Initializes the LCD driver once at startup
 *  - Blocks waiting for new thermostat_state_t messages on g_q_thermostat_state
 *  - Renders the most recent state on the LCD whenever it changes
 *  - Feeds the watchdog regularly
 */

 
static void task_display(void *arg)
{
    (void)arg;

    // Register this task with watchdog so lockups can be detected.
    watchdog_register_current("DISPLAY");

    // Initialize LCD driver hardware. If this fails, we log an error
    // and keep running the task, but nothing will be drawn.
    app_error_t err = drv_display_init();
    if (err != ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "drv_display_init failed, err=%d", (int)err);
        // We do NOT fatal here, because the rest of the system
        // (control, sensors, etc.) can still keep running.
    }

    thermostat_state_t state;

    while (1) {
        // Wait for the next state update from thermostat core.
        if (xQueueReceive(g_q_thermostat_state, &state, portMAX_DELAY) == pdTRUE) {

            // If init succeeded, this will render Tin/Tout/setpoint/output
            // on the LCD. If init failed earlier, drv_display_show_state
            // should ideally be safe to call and simply return an error.
            drv_display_show_state(&state);
        }

        // Feed watchdog after each update cycle.
        watchdog_feed();
    }
}

void task_display_start(void)
{
    xTaskCreate(
        task_display,
        "task_display",
        TASK_STACK_DISPLAY,   // define in core/config.h
        NULL,
        TASK_PRIO_DISPLAY,    // define in core/config.h
        NULL
    );
}
