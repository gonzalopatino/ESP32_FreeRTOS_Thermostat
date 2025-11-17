#ifndef THERMOSTAT_CONFIG_H
#define THERMOSTAT_CONFIG_H

#include "core/app_types.h"   // if you want app_error_t here


/**
 * @brief Structure holding thermostat control configuration.
 *
 * This will eventually be updated from:
 *  - user interface (buttons or encoder)
 *  - cloud commands
 *  - stored NVS configuration
 *
 * For now it just wraps the defaults from config.h.
 */
typedef struct {
    float setpoint_c;      // Desired indoor temperature in Celsius
    float hysteresis_c;    // Deadband around setpoint
} thermostat_config_t;

/**
 * @brief Initialize the thermostat configuration subsystem.
 *
 * Loads default values from config.h and prepares any internal
 * synchronization primitives (such as a mutex) that protect the
 * configuration structure.
 */
app_error_t thermostat_config_init(void);

/**
 * @brief Get a snapshot of the current thermostat configuration.
 *
 * Thread safe. Copies the internal config into @p out_cfg.
 *
 * @param[out] out_cfg Pointer to caller allocated struct.
 * @return ERR_OK on success, ERR_GENERIC if out_cfg is NULL.
 */
app_error_t thermostat_config_get(thermostat_config_t *out_cfg);

/**
 * @brief Update the thermostat configuration at runtime.
 *
 * Intended to be called from UI or network tasks when the user changes
 * the setpoint or hysteresis.
 *
 * @param[in] new_cfg New configuration to apply.
 * @return ERR_OK on success, ERR_GENERIC if new_cfg is NULL.
 */
app_error_t thermostat_config_set(const thermostat_config_t *new_cfg);

#endif  // THERMOSTAT_CONFIG_H
