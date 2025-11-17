#include "core/thermostat.h"
#include "core/thermostat_config.h"
#include "core/config.h"
#include "core/error.h"
#include "core/logging.h"

static const char *TAG = "TH_CORE";

// Internal state for the thermostat brain.
// This holds the last decision and data for telemetry / UI.
static thermostat_state_t s_state;
static bool s_initialized = false;

/**
 * @brief Initialize thermostat core and underlying configuration.
 */
app_error_t thermostat_core_init(void)
{
    app_error_t err;
    thermostat_config_t cfg;

    // Bring up the config subsystem (NVS / defaults / etc.).
    err = thermostat_config_init();
    if (err != ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "thermostat_config_init failed, err=%d", (int)err);
        return err;
    }

    // Try to read current config; if this fails, fall back to compile-time defaults.
    err = thermostat_config_get(&cfg);
    if (err != ERR_OK) {
        cfg.setpoint_c   = THERMOSTAT_SETPOINT_C;
        cfg.hysteresis_c = THERMOSTAT_HYSTERESIS_C;
    }

    // Initialize internal state.
    s_state.mode         = THERMOSTAT_MODE_HEAT;          // simple heating system
    s_state.output       = THERMOSTAT_OUTPUT_HEAT_OFF;    // never start with heater ON
    s_state.setpoint_c   = cfg.setpoint_c;
    s_state.hysteresis_c = cfg.hysteresis_c;
    s_state.tin_c        = 0.0f;
    s_state.tout_c       = 0.0f;
    s_state.timestamp_ms = 0u;

    s_initialized = true;

    log_post(LOG_LEVEL_INFO, TAG,
             "Core init: mode=HEAT sp=%.2fC hyst=%.2fC",
             s_state.setpoint_c, s_state.hysteresis_c);

    return ERR_OK;
}

/**
 * @brief Apply hysteresis control for heating mode.
 *
 * This function:
 *  - Reads latest configuration (setpoint/hysteresis) via thermostat_config_get()
 *  - Uses previous output (s_state.output) to keep behavior stable inside the band
 *  - Updates internal state and returns a snapshot to the caller
 */
app_error_t thermostat_core_process_sample(
    const sensor_sample_t *sample,
    thermostat_state_t    *out_state)
{
    if (sample == NULL || out_state == NULL) {
        return ERR_GENERIC;
    }

    if (!s_initialized) {
        log_post(LOG_LEVEL_ERROR, TAG, "Core used before init");
        return ERR_GENERIC;
    }

    thermostat_config_t cfg;
    app_error_t err = thermostat_config_get(&cfg);
    if (err != ERR_OK) {
        // Fall back to compile-time defaults if config read fails.
        cfg.setpoint_c   = THERMOSTAT_SETPOINT_C;
        cfg.hysteresis_c = THERMOSTAT_HYSTERESIS_C;
    }

    const float sp   = cfg.setpoint_c;
    const float hyst = cfg.hysteresis_c;
    const float tin  = sample->temp_inside_c;
    const float tout = sample->temp_outside_c;

    // Start from previous output to preserve hysteresis behavior.
    thermostat_output_t output = s_state.output;

    if (s_state.mode == THERMOSTAT_MODE_OFF) {
        // In OFF mode, heater is always off regardless of temperature.
        output = THERMOSTAT_OUTPUT_HEAT_OFF;

    } else if (s_state.mode == THERMOSTAT_MODE_HEAT) {
        // Heating hysteresis rule:
        //  - If Tin < (sp - hyst): heater ON
        //  - If Tin > (sp + hyst): heater OFF
        //  - Else: keep previous output (inside deadband)
        if (tin < (sp - hyst)) {
            output = THERMOSTAT_OUTPUT_HEAT_ON;
        } else if (tin > (sp + hyst)) {
            output = THERMOSTAT_OUTPUT_HEAT_OFF;
        } else {
            // Inside deadband, do not flip output.
        }
    }

    // Update internal snapshot.
    s_state.setpoint_c   = sp;
    s_state.hysteresis_c = hyst;
    s_state.tin_c        = tin;
    s_state.tout_c       = tout;
    s_state.timestamp_ms = sample->timestamp_ms;
    s_state.output       = output;

    // Mode remains whatever was last set (only HEAT for now).
    // In the future, UI / MQTT could call a mode setter.

    // Return copy to caller.
    *out_state = s_state;

    return ERR_OK;
}
