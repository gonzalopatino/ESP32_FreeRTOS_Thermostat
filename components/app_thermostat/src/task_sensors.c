#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/error.h"
#include "app/task_common.h"
#include "drivers/drv_temp_sensors.h"

#include "core/timeutil.h" //for real time clock (RTO)

//Gonzalo

/**
 * @brief FreeRTOS task responsible for reading temperature sensors.
 *
 * This task periodically:
 *   1. Reads indoor/outdoor temperature samples from the driver
 *   2. Pushes the sample into a shared queue for the control task
 *   3. Logs raw sensor data (debug level)
 *   4. Feeds the watchdog to indicate it is alive
 *
 * The driver currently generates fake data (stub) until real hardware
 * support (I2C/ADC sensors) is implemented. The rest of the system can
 * be developed independently thanks to this stub behavior.
 */
static void task_sensors(void *arg) {
    (void)arg;   // Prevent unused parameter warning

    // Register this task in the watchdog framework.
    // If this task stops feeding the watchdog, the system will reset.
    watchdog_register_current("SENSORS");

    // Initialize sensor driver (stub today, real hardware later).
    if (drv_temp_sensors_init() != ERR_OK) {
        error_report(ERR_GENERIC, "drv_temp_sensors_init");
    }

    // Using vTaskDelayUntil ensures consistent periodic execution,
    // removing drift that occurs with vTaskDelay.
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {

        // Structure to hold the temperature sample.
        // Zero-initialize to avoid garbage if fields are added later.
        sensor_sample_t sample = {0};

        // Ask driver for new readings (stub simulates drifting values)
        app_error_t err = drv_temp_read(&sample);
        if (err == ERR_OK) {

            // If the queue exists, overwrite with the latest sample.
            // Overwrite is intentional: control logic needs ONLY
            // the newest sample, not a backlog of old temperatures.
            if (g_q_sensor_samples) {
                xQueueOverwrite(g_q_sensor_samples, &sample);
            }

            // Log raw sensor readings for debugging / calibration
            char iso[32];
            if(timeutil_get_iso8601(iso, sizeof(iso))){
                log_post(LOG_LEVEL_DEBUG, "SENSORS",
                    "Tin=%.2fC Tout=%.2fC t=%lu ms local=%s",
                    sample.temp_inside_c,
                    sample.temp_outside_c,
                    (unsigned long)sample.timestamp_ms,
                    iso);
            } else {
                //Time not set yet, log without Local time
                log_post(LOG_LEVEL_DEBUG, "SENSORS",
                    "Tin=%.2fC Tout=%.2fC t=%lu ms (no RTC yet)",
                    sample.temp_inside_c,
                    sample.temp_outside_c,
                    (unsigned long)sample.timestamp_ms);
            }
 

        } else {
            // If driver fails, report non-fatal error (logged only)
            error_report(err, "drv_temp_read");
        }

        // Notify watchdog that this task is alive and progressing
        watchdog_feed();

        // Sleep until next cycle (e.g., PERIOD_SENSORS_MS = 500 ms)
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PERIOD_SENSORS_MS));
    }
}


/**
 * @brief Create and start the sensor task.
 *
 * Called during system initialization (from app_main).
 * Stack size and priority are configured centrally in config.h to keep
 * tuning easy and consistent across the application.
 */
void task_sensors_start(void) {
    xTaskCreate(
        task_sensors,         // Task function
        "task_sensors",       // Task name (debugging)
        TASK_STACK_SENSORS,   // Stack size (words)
        NULL,                 // No parameters
        TASK_PRIO_SENSORS,    // Task priority
        NULL                  // No task handle needed
    );
}
