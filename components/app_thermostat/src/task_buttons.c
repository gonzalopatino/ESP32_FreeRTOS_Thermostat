#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/error.h"

#include "core/thermostat_config.h"
#include "drivers/drv_buttons.h"

#include "app/task_buttons.h"

static const char *TAG = "BTN_UI";

static void apply_setpoint_delta(float delta_c)
{
    thermostat_config_t cfg;
    if (thermostat_config_get(&cfg) != ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_get");
        return;
    }

    cfg.setpoint_c += delta_c;

    if (cfg.setpoint_c < THERMOSTAT_SP_MIN_C) {
        cfg.setpoint_c = THERMOSTAT_SP_MIN_C;
    } else if (cfg.setpoint_c > THERMOSTAT_SP_MAX_C) {
        cfg.setpoint_c = THERMOSTAT_SP_MAX_C;
    }

    if (thermostat_config_set(&cfg) != ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_set");
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG,
             "Setpoint changed to %.1f C (delta=%.1f)",
             cfg.setpoint_c, delta_c);
}

/**
 * @brief Task that consumes button events and adjusts thermostat setpoint.
 *
 * Uses a simple time-based debounce in task context.
 */
static void task_buttons(void *arg)
{
    (void)arg;

    watchdog_register_current("BUTTONS");

    if (drv_buttons_init() != ERR_OK) {
        error_fatal(ERR_GENERIC, "drv_buttons_init");
    }

    QueueHandle_t q = drv_buttons_get_queue();
    if (q == NULL) {
        error_fatal(ERR_GENERIC, "drv_buttons_get_queue");
    }

    // Debounce: remember last accepted event time per button
    TickType_t last_up_ticks   = 0;
    TickType_t last_down_ticks = 0;

    while (1) {
        button_event_t evt;
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();
            TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);

            if (evt == BUTTON_EVENT_UP) {
                if ((now - last_up_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(+THERMOSTAT_SP_STEP_C);
                    last_up_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "UP ignored (debounce)");
                }
            } else if (evt == BUTTON_EVENT_DOWN) {
                if ((now - last_down_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(-THERMOSTAT_SP_STEP_C);
                    last_down_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "DOWN ignored (debounce)");
                }
            }

            watchdog_feed();
        }
    }
}

void task_buttons_start(void)
{
    xTaskCreate(
        task_buttons,
        "task_buttons",
        TASK_STACK_BUTTONS,
        NULL,
        TASK_PRIO_BUTTONS,
        NULL
    );
}
