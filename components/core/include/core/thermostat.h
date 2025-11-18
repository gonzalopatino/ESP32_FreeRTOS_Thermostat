#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <stdint.h>
#include <stdbool.h>

#include "core/app_types.h"       // sensor_sample_t, app_error_t
#include "core/error.h"           // app_error_t
#include "core/thermostat_config.h" //thermostat_config_t (if needed)

/**
 * @brief High-level operating mode for the thermostat.
 *
 * For now, only HEAT mode is used. OFF and COOL are reserved for
 * future expansion (e.g., adding AC).
 */
typedef enum {
    THERMOSTAT_MODE_OFF  = 0,
    THERMOSTAT_MODE_HEAT = 1,
    THERMOSTAT_MODE_COOL = 2,
    THERMOSTAT_MODE_AUTO = 3, //Reserved /future
} thermostat_mode_t;

/**
 * @brief Output command from the thermostat brain.
 *
 * This is deliberately abstract: the CONTROL task will translate this
 * to GPIO, relays, LEDs, etc.
 */
typedef enum {
    THERMOSTAT_OUTPUT_OFF      = 0,
    THERMOSTAT_OUTPUT_HEAT_ON  = 1,
    THERMOSTAT_OUTPUT_COOL_ON  = 2,
} thermostat_output_t;

/**
 * @brief Snapshot of thermostat state.
 *
 * This is what UI / telemetry / logging can use to show:
 *  - temperatures
 *  - config (setpoint, hysteresis)
 *  - operating mode
 *  - current output (heat on/off)
 */
typedef struct {
    thermostat_mode_t   mode;          // e.g., HEAT, OFF
    thermostat_output_t output;        // HEAT_ON / HEAT_OFF

    float setpoint_c;                  // current setpoint
    float hysteresis_c;                // current hysteresis

    float tin_c;                       // indoor temp from last sample
    float tout_c;                      // outdoor temp (if available)

    uint32_t timestamp_ms;             // timestamp of last sample
} thermostat_state_t;

/**
 * @brief Initialize thermostat core.
 *
 * Responsibilities:
 *  - Initialize underlying configuration (thermostat_config)
 *  - Load initial setpoint/hysteresis from defaults or stored config
 *  - Initialize internal state with HEAT_OFF, valid config
 */
app_error_t thermostat_core_init(void);

/**
 * @brief Process a new sensor sample and compute the next action.
 *
 * Implements the thermostat decision logic:
 *  - Uses current configuration (setpoint, hysteresis)
 *  - Applies hysteresis rules around the setpoint
 *  - Updates and returns a snapshot of the internal state
 *
 * @param[in]  sample     Latest sensor reading (Tin/Tout/timestamp)
 * @param[out] out_state  Snapshot of updated thermostat state
 *
 * @return ERR_OK on success, ERR_GENERIC if arguments are invalid
 *         or the module was not initialized.
 */
app_error_t thermostat_core_process_sample(
    const sensor_sample_t *sample,
    thermostat_state_t    *out_state
);

#endif  // THERMOSTAT_H
