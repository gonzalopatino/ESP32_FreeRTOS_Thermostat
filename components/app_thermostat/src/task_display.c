/**
 * @file    task_display.c
 * @brief   LCD display update task.
 *
 * Updates the 16x2 LCD display with current thermostat status
 * including temperature, setpoint, mode, and output state.
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * INCLUDES
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/thermostat.h"

#include "app/task_common.h"
#include "app/task_display.h"
#include "app/setup_server.h"

#include "drivers/drv_display.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE VARIABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *TAG = "DISPLAY";

static void task_display(void *arg)
{
    (void)arg;

    // Register this task in the watchdog
    watchdog_register_current("DISPLAY");
    log_post(LOG_LEVEL_INFO, TAG, "DISPLAY task starting");

    if (drv_display_init() != APP_ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "LCD init failed");
        // Bail out but keep feeding watchdog slowly
        while (1) {
            watchdog_feed();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    thermostat_state_t state;
    

    while (1) {
        // Block until CONTROL publishes a new state
        if (xQueueReceive(g_q_thermostat_state, &state, portMAX_DELAY) == pdTRUE) {

            // Skip LCD updates if in settings mode (buttons task controls LCD)
            if (setup_server_is_settings_mode()) {
                watchdog_feed();
                continue;
            }

            log_post(LOG_LEVEL_DEBUG, TAG,
                     "DISPLAY got state: Tin=%.2f Tout=%.2f sp=%.2f hyst=%.2f out=%d",
                     state.tin_c,
                     state.tout_c,
                     state.setpoint_c,
                     state.hysteresis_c,
                     (int)state.output);

            // Render that state to the LCD
            drv_display_show_state(&state);

            watchdog_feed();
        }
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
