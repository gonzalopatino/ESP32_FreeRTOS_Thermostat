#include "drivers/drv_temp_sensors.h"
#include "core/logging.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/**
 *  @brief Initialize the temperature sensor subsystem.
 * 
 * This is currently a stub. Later this function will:
 *  - configure I2C or ADC peripherals
 *  - detect and initialize actual temperature sensor hardware
 *  - perform calibration or startup routines
 * 
 * Returning ERR_OK allows the rest of the system to proceed even though
 * the real hardware code is not implemented yet
 */

app_error_t drv_temp_sensors_init(void) {
    // Later: unit I2C, ADC, actual sensor chips
    log_post(LOG_LEVEL_INFO, 
        "DRV_TS", 
        "Temperature snesors init stub");

    return ERR_OK; // Stub always succeeds for now
}

/**
 * @brief Read indoor and outdoor temperatures.
 *
 * This is a **fake data generator** used while hardware drivers are not
 * implemented yet. It simulates smoothly changing temperatures to
 * allow the rest of the system (control logic, display, telemetry)
 * to be developed and tested independently.
 *
 * @param[out] out_sample  Pointer to a sample structure filled by this function.
 *
 * @return ERR_OK on success,
 *         ERR_GENERIC if out_sample is NULL.
 */

app_error_t drv_temp_read (sensor_sample_t *out_sample) {
    if (!out_sample) {
        return ERR_GENERIC; // Invalid argument protection
    }

    // --- Fake temperature signals ---
    //
    // These static variables keep their values between calls,
    // simulating slowly changing indoor/outdoor temperatures.
    static float tin = 21.5f;
    static float tout = 10.0f;

     // Simulate indoor temperature drifting up, then wrapping
    tin += 0.1f;
    if (tin > 24.0f) {
        tin = 21.5f;
    }

    // Simulate outdoor temperature with smaller drift
    tout += 0.05f;
    if (tout > 12.0f) {
        tout = 10.0f;
    }

    // Fill the output structure with the fake sensor data
    out_sample->temp_inside_c = tin;
    out_sample->temp_outside_c = tout;

    // Timestamp using FreeRTOS tick counter (converted to ms)
    out_sample->timestamp_ms =
        (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    return ERR_OK;
}