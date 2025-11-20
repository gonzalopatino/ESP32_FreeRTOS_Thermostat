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
    if (err != APP_ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "thermostat_config_init failed, err=%d", (int)err);
        return err;
    }

    // Try to read current config; if this fails, fall back to compile-time defaults.
    err = thermostat_config_get(&cfg);
    if (err != APP_ERR_OK) {
        cfg.setpoint_c   = THERMOSTAT_SETPOINT_C;
        cfg.hysteresis_c = THERMOSTAT_HYSTERESIS_C;
    }

    // Initialize internal state.
    // Fow now, use HEAT MODE only so behavior matches your current hardware
    s_state.mode         = THERMOSTAT_MODE_HEAT;      // default
    s_state.output       = THERMOSTAT_OUTPUT_OFF;     // never start ON
    s_state.setpoint_c   = cfg.setpoint_c;
    s_state.hysteresis_c = cfg.hysteresis_c;
    s_state.tin_c        = 0.0f;
    s_state.tout_c       = 0.0f;
    s_state.timestamp_ms = 0u;

    s_initialized = true;

    log_post(LOG_LEVEL_INFO, TAG,
             "Core init: mode=AUTO sp=%.2fC hyst=%.2fC",
             s_state.setpoint_c, s_state.hysteresis_c);

    return APP_ERR_OK;
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
    if (err != APP_ERR_OK) {
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
    thermostat_mode_t mode = s_state.mode;

    switch (mode) {

        case THERMOSTAT_MODE_OFF:
            // Everything off regardless of temperature.
            output = THERMOSTAT_OUTPUT_OFF;
            break;

        case THERMOSTAT_MODE_HEAT:
            // Heating hysteresis:
            //  Tin < (sp - hyst)  -> HEAT_ON
            //  Tin > (sp + hyst)  -> OFF
            //  otherwise          -> keep output
            if (tin < (sp - hyst)) {
                output = THERMOSTAT_OUTPUT_HEAT_ON;
            } else if (tin > (sp + hyst)) {
                output = THERMOSTAT_OUTPUT_OFF;
            } else {
                // inside band: keep output
            }
            break;

        case THERMOSTAT_MODE_COOL:
            // Cooling hysteresis:
            //  Tin > (sp + hyst)  -> COOL_ON
            //  Tin < (sp - hyst)  -> OFF
            //  otherwise          -> keep output
            if (tin > (sp + hyst)) {
                output = THERMOSTAT_OUTPUT_COOL_ON;
            } else if (tin < (sp - hyst)) {
                output = THERMOSTAT_OUTPUT_OFF;
            } else {
                // inside band: keep output
            }
            break;

        case THERMOSTAT_MODE_AUTO:
            // Symmetric auto band:
            //  Tin < (sp - hyst)  -> HEAT_ON
            //  Tin > (sp + hyst)  -> COOL_ON
            //  otherwise          -> OFF
            if (tin < (sp - hyst)) {
                output = THERMOSTAT_OUTPUT_HEAT_ON;
            } else if (tin > (sp + hyst)) {
                output = THERMOSTAT_OUTPUT_COOL_ON;
            } else {
                output = THERMOSTAT_OUTPUT_OFF;
            }
            break;

        default:
            // Fail safe
            output = THERMOSTAT_OUTPUT_OFF;
            break;
        }

    // Update snapshot
    s_state.setpoint_c   = sp;
    s_state.hysteresis_c = hyst;
    s_state.tin_c        = tin;
    s_state.tout_c       = tout;
    s_state.timestamp_ms = sample->timestamp_ms;
    s_state.output       = output;
    s_state.mode         = mode;

    

    // Mode remains whatever was last set (only HEAT for now).
    // In the future, UI / MQTT could call a mode setter.

    // Return copy to caller.
    *out_state = s_state;

    return APP_ERR_OK;
}

/**
 * @brief Set the operating mode (OFF / HEAT / COOL / AUTO).
 */

// -----------------------------------------------------------------------------
// Mode and state access API
// -----------------------------------------------------------------------------

app_error_t thermostat_set_mode(thermostat_mode_t mode)
{
    if (!s_initialized) {
        log_post(LOG_LEVEL_ERROR, TAG, "thermostat_set_mode called before init");
        return ERR_GENERIC;
    }

    switch (mode) {
    case THERMOSTAT_MODE_OFF:
    case THERMOSTAT_MODE_HEAT:
    case THERMOSTAT_MODE_COOL:
    case THERMOSTAT_MODE_AUTO:
        // valid modes
        s_state.mode = mode;

        // In OFF mode, force outputs OFF immediately
        if (mode == THERMOSTAT_MODE_OFF) {
            s_state.output = THERMOSTAT_OUTPUT_OFF;
        }

        log_post(LOG_LEVEL_INFO, TAG, "Mode set to %d", (int)mode);
        return APP_ERR_OK;

    default:
        log_post(LOG_LEVEL_ERROR, TAG,
                 "thermostat_set_mode: invalid mode=%d", (int)mode);
        return ERR_GENERIC;
    }
}

/**
 * @brief Get current operating mode.
 */
app_error_t thermostat_get_mode(thermostat_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return ERR_GENERIC;
    }

    if (!s_initialized) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "thermostat_get_mode called before init");
        return ERR_GENERIC;
    }

    *out_mode = s_state.mode;
    return APP_ERR_OK;
}

/**
 * @brief Get a snapshot of the current thermostat state.
 *
 * Thread-safe in the sense that s_state is only updated inside the core
 * and this function returns a simple struct copy.
 */
app_error_t thermostat_get_state_snapshot(thermostat_state_t *out_state)
{
    if (!out_state) {
        return ERR_GENERIC;
    }
    if (!s_initialized) {
        return ERR_GENERIC;
    }

    *out_state = s_state;
    return APP_ERR_OK;
}
