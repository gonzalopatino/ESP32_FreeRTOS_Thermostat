#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/app_types.h"

#include "app/task_common.h"   // g_q_sensor_samples
#include "app/task_control.h"

#include <stdio.h>  // for snprintf


static const char *TAG = "CONTROL";


// Simple enum to represent heater output state.
// This keeps logic readable and avoids using raw 0/1 magic values.
typedef enum {
    HEAT_OFF = 0,
    HEAT_ON  = 1
} heat_state_t;


/**
 * @brief Configure the GPIO pin used to drive the heating output.
 *
 * This function:
 *   - sets the configured pin as an output
 *   - disables internal pull resistors
 *   - ensures the heater starts in the OFF state
 *
 * Requires that GPIO_HEAT_OUTPUT is defined (for example in config.h or BSP).
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
 * @brief Apply the desired heat state to the hardware output.
 *
 * Wrapper around gpio_set_level so higher-level logic deals only
 * with heat_state_t and not raw GPIO values.
 */
static void apply_heat_output(heat_state_t state) {
    gpio_set_level(GPIO_HEAT_OUTPUT, (state == HEAT_ON) ? 1 : 0);
}


/**
 * @brief Thermostat control task.
 *
 * Responsibilities:
 *   - Wait for new sensor samples from g_q_sensor_samples
 *   - Compare indoor temperature against setpoint + hysteresis band
 *   - Decide whether to turn HEAT_ON or HEAT_OFF
 *   - Drive heater GPIO accordingly
 *   - Log decisions (INFO on state change, DEBUG on keep-state)
 *   - Feed watchdog regularly
 *
 * This implements a simple ON/OFF controller with hysteresis,
 * not full PID (yet), which is appropriate for many thermostats.
 */
static void task_control(void *arg) {
    (void)arg;

    // Register this task with the watchdog framework so the system
    // can detect if control logic stops running.
    watchdog_register_current("CONTROL");

    // Ensure heater GPIO is configured and OFF.
    control_gpio_init();

    sensor_sample_t sample;
    heat_state_t heat_state = HEAT_OFF;

    // Buffer for formatted log messages.
    char msg_buf[128];

    while (1) {

        // Block until we receive a new sensor sample from the SENSORS task.
        // The SENSORS task uses xQueueOverwrite, so this will always give us
        // the most recent reading.
        if (xQueueReceive(g_q_sensor_samples, &sample, portMAX_DELAY) == pdTRUE) {

            // Use your established sensor_sample_t field names.
            // Adjust if your struct differs.
            float tin  = sample.temp_inside_c;
            float tout = sample.temp_outside_c;  // currently not used in logic, but useful for logs

            float sp   = THERMOSTAT_SETPOINT_C;     // desired temperature
            float hyst = THERMOSTAT_HYSTERESIS_C;   // hysteresis band

            heat_state_t prev_state = heat_state;

            // --- Hysteresis decision logic ---
            //
            // If temperature is below (setpoint - hysteresis) → turn heat ON.
            // If temperature is above (setpoint + hysteresis) → turn heat OFF.
            // Otherwise stay in the previous state (inside the deadband).
            if (tin < (sp - hyst)) {
                heat_state = HEAT_ON;
            } else if (tin > (sp + hyst)) {
                heat_state = HEAT_OFF;
            } else {
                // Inside deadband: keep previous state.
            }

            // Only drive GPIO and emit INFO log when the state actually changes.
            if (heat_state != prev_state) {
                apply_heat_output(heat_state);

                snprintf(msg_buf, sizeof(msg_buf),
                    "Tin=%.2fC Tout=%.2fC sp=%.2fC hyst=%.2fC action=%s",
                    tin, tout, sp, hyst,
                    (heat_state == HEAT_ON) ? "HEAT_ON" : "HEAT_OFF");

                // INFO log on state change.
                log_post(LOG_LEVEL_INFO, TAG, "%s", msg_buf);

            } else {
                // Optional: DEBUG log when we keep the same state.
                snprintf(msg_buf, sizeof(msg_buf),
                    "Tin=%.2fC Tout=%.2fC sp=%.2fC hyst=%.2fC action=KEEP_%s",
                    tin, tout, sp, hyst,
                    (heat_state == HEAT_ON) ? "HEAT_ON" : "HEAT_OFF");

                log_post(LOG_LEVEL_INFO, TAG, "%s", msg_buf);
            }

            // Feed watchdog after completing a control cycle.
            watchdog_feed();
        }
    }
}


/**
 * @brief Create and start the control task.
 *
 * Called from app_main() once shared queues, logging, and sensors
 * are set up. Stack size and priority are configured in config.h
 * via TASK_STACK_CONTROL and TASK_PRIO_CONTROL.
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
