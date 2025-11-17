#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/app_types.h"
#include "core/error.h"

#include "core/thermostat.h"      // thermostat_core_init, thermostat_core_process_sample

#include "app/task_common.h"      // g_q_sensor_samples
#include "app/task_control.h"

#include "core/thermostat_config.h"
#include "core/thermostat.h"


#include <stdio.h>                // snprintf

static const char *TAG = "CONTROL";

/**
 * @brief Configure the GPIO pin used to drive the heating output.
 *
 * This function:
 *   - sets the configured pin as an output
 *   - disables internal pull resistors
 *   - ensures the heater starts in the OFF state
 *
 * Requires that GPIO_HEAT_OUTPUT is defined in core/config.h
 * or an equivalent board configuration header.
 */
static void control_gpio_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_HEAT_OUTPUT,   // Select heater control pin
        .mode         = GPIO_MODE_OUTPUT,           // Output mode
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE           // No interrupts for this pin
    };
    gpio_config(&io_conf);

    // Start with heater OFF so we never boot into an ON state accidentally.
    gpio_set_level(GPIO_HEAT_OUTPUT, 0);
}

/**
 * @brief Apply thermostat output to the physical heater GPIO.
 *
 * The thermostat core returns an abstract output (HEAT_ON/OFF),
 * and this function translates that into actual GPIO levels.
 */
static void apply_heat_output(thermostat_output_t output) {
    int level = (output == THERMOSTAT_OUTPUT_HEAT_ON) ? 1 : 0;
    gpio_set_level(GPIO_HEAT_OUTPUT, level);
}

/**
 * @brief Thermostat CONTROL task.
 *
 * Responsibilities:
 *   - Wait for new sensor samples from g_q_sensor_samples
 *   - Pass the samples into the thermostat core
 *   - Apply the resulting output to the heater GPIO
 *   - Log decisions (INFO on state change, DEBUG or INFO on keep-state)
 *   - Feed watchdog regularly
 *
 * CONTROL is now a thin adapter between:
 *   - the RTOS / queues / hardware
 *   - the thermostat decision logic in core/thermostat.c
 */
static void task_control(void *arg) {
    (void)arg;

    // Register this task with the watchdog framework so the system
    // can detect if control logic stops running.
    watchdog_register_current("CONTROL");

    // Ensure heater GPIO is configured and OFF.
    control_gpio_init();

    sensor_sample_t     sample;
    thermostat_state_t  th_state;
    thermostat_output_t prev_output = THERMOSTAT_OUTPUT_HEAT_OFF;

    char msg_buf[128];  // Buffer for formatted log messages.

    while (1) {
        // Block until we receive a new sensor sample from the SENSORS task.
        // The SENSORS task uses xQueueOverwrite, so this will always give us
        // the most recent reading.
        if (xQueueReceive(g_q_sensor_samples, &sample, portMAX_DELAY) == pdTRUE) {

            app_error_t err = thermostat_core_process_sample(&sample, &th_state);
            if (err != ERR_OK) {
                // If the brain fails, report the error and skip this cycle.
                error_report(err, "thermostat_core_process_sample");
                watchdog_feed();
                continue;
            }

            //NEW: publish the state snapshot for UI / Telemetry
            if (g_q_thermostat_state != NULL) {
                xQueueOverwrite(g_q_thermostat_state, &th_state);
            }

            // Apply new output if it changed.
            if (th_state.output != prev_output) {
                apply_heat_output(th_state.output);

                snprintf(msg_buf, sizeof(msg_buf),
                         "Tin=%.2fC Tout=%.2fC sp=%.2fC hyst=%.2fC action=%s",
                         th_state.tin_c,
                         th_state.tout_c,
                         th_state.setpoint_c,
                         th_state.hysteresis_c,
                         (th_state.output == THERMOSTAT_OUTPUT_HEAT_ON)
                            ? "HEAT_ON"
                            : "HEAT_OFF");

                log_post(LOG_LEVEL_INFO, TAG, "%s", msg_buf);
                prev_output = th_state.output;

            } else {
                // Optional: log when we keep the same state.
                snprintf(msg_buf, sizeof(msg_buf),
                         "Tin=%.2fC Tout=%.2fC sp=%.2fC hyst=%.2fC action=KEEP_%s",
                         th_state.tin_c,
                         th_state.tout_c,
                         th_state.setpoint_c,
                         th_state.hysteresis_c,
                         (th_state.output == THERMOSTAT_OUTPUT_HEAT_ON)
                            ? "HEAT_ON"
                            : "HEAT_OFF");

                log_post(LOG_LEVEL_DEBUG, TAG, "%s", msg_buf);
            }

            // Feed watchdog after completing a control cycle.
            watchdog_feed();
        }
    }
}

/**
 * @brief Create and start the CONTROL task.
 *
 * Called from app_main() once shared queues, logging, sensors,
 * and thermostat core have been initialized. Stack size and priority
 * are configured in config.h via TASK_STACK_CONTROL and TASK_PRIO_CONTROL.
 */
void task_control_start(void) {
    xTaskCreate(
        task_control,          // Task entry function
        "task_control",        // Task name (for debugging)
        TASK_STACK_CONTROL,    // Stack size (words) from config.h
        NULL,                  // Task parameter
        TASK_PRIO_CONTROL,     // Priority from config.h
        NULL                   // No handle stored
    );
}
