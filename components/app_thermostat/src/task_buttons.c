#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/error.h"

#include "core/thermostat_config.h"
#include "drivers/drv_buttons.h"
#include "core/thermostat.h" // <-- for thermostat_get_mode / thermostat_set_mode

#include "app/task_buttons.h"

static const char *TAG = "BTN_UI";



/* ---------------- Setpoint helper ---------------- */

static void apply_setpoint_delta(float delta_c)
{
    thermostat_config_t cfg;
    if (thermostat_config_get(&cfg) != ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_get");
        return;
    }

    cfg.setpoint_c += delta_c;

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
 * @brief Cycle mode: HEAT -> COOL -> OFF -> HEAT ...
 */
static const char *mode_to_str(thermostat_mode_t mode)
{
    switch (mode) {
    case THERMOSTAT_MODE_OFF:  return "OFF";
    case THERMOSTAT_MODE_HEAT: return "HEAT";
    case THERMOSTAT_MODE_COOL: return "COOL";
    case THERMOSTAT_MODE_AUTO: return "AUTO";
    default:                   return "UNKNOWN";
    }
}
 
static void cycle_mode(void)
{
    thermostat_mode_t current;
    if (thermostat_get_mode(&current) != ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_get_mode");
        return;
    }

    thermostat_mode_t next = THERMOSTAT_MODE_HEAT;

    switch (current) {
    case THERMOSTAT_MODE_HEAT:
        next = THERMOSTAT_MODE_COOL;
        break;
    case THERMOSTAT_MODE_COOL:
        next = THERMOSTAT_MODE_OFF;
        break;
    case THERMOSTAT_MODE_OFF:
        next = THERMOSTAT_MODE_AUTO;
        break;
    case THERMOSTAT_MODE_AUTO:
        next = THERMOSTAT_MODE_HEAT;
    default:
        next = THERMOSTAT_MODE_HEAT;
        break;
    }

    if (thermostat_set_mode(next) != ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_set_mode");
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG,
             "Mode changed: %s -> %s",
             mode_to_str(current),
             mode_to_str(next));
}

/* ---------------- Task ---------------- */

/**
 * @brief Task that consumes button events and adjusts thermostat setpoint / mode.
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
    TickType_t last_mode_ticks = 0;

    const TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);

    while (1) {
        button_event_t evt;
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();

            switch (evt) {
            case BUTTON_EVENT_UP:
                if ((now - last_up_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(+THERMOSTAT_SP_STEP_C);
                    last_up_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "UP ignored (debounce)");
                }
                break;

            case BUTTON_EVENT_DOWN:
                if ((now - last_down_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(-THERMOSTAT_SP_STEP_C);
                    last_down_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "DOWN ignored (debounce)");
                }
                break;

            case BUTTON_EVENT_MODE:
                if ((now - last_mode_ticks) >= debounce_ticks) {
                    cycle_mode();
                    last_mode_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "MODE ignored (debounce)");
                }
                break;

            default:
                // Unknown event, ignore
                break;
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
