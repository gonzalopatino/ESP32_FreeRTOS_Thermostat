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
 * This is abstract. The CONTROL task decides how to map these to GPIO/relays.
 *
 * For now:
 *  - Heater is driven only when THERMOSTAT_OUTPUT_HEAT_ON.
 *  - COOL outputs are "no-op" until you wire an AC output pin.
 */
typedef enum {
    THERMOSTAT_OUTPUT_OFF = 0,
    THERMOSTAT_OUTPUT_HEAT_ON,
    THERMOSTAT_OUTPUT_COOL_ON
} thermostat_output_t;

/**
 * @brief Snapshot of thermostat state.
 *
 * This is what UI / telemetry / logging can use to show:
 *  - temperatures
 *  - config (setpoint, hysteresis)
 *  - operating mode
 *  - current output (heat/cool on/off)
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
 * @return ERR_OK on success, ERR_GENERIC on invalid args or uninitialized core.
 */
app_error_t thermostat_core_process_sample(
    const sensor_sample_t *sample,
    thermostat_state_t    *out_state
);

/**
 * @brief Set the operating mode (OFF / HEAT / COOL / AUTO).
 *
 * Safe to call from tasks such as:
 *  - buttons task
 *  - MQTT command handler
 *  - UI task
 *
 * @param mode New mode to apply.
 * @return ERR_OK on success, ERR_GENERIC if core not initialized or mode invalid.
 */
app_error_t thermostat_set_mode(thermostat_mode_t mode);

/**
 * @brief Get a snapshot of the current thermostat state.
 *
 * Useful for UI or telemetry tasks that want the latest state without
 * waiting on a queue.
 *
 * @param[out] out_state Caller-allocated struct to receive the snapshot.
 * @return ERR_OK on success, ERR_GENERIC if out_state is NULL or core not initialized.
 */
app_error_t thermostat_get_state(thermostat_state_t *out_state);

#endif  // THERMOSTAT_H
