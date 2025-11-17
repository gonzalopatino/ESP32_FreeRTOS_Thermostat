#include "drivers/drv_display.h"
#include "core/logging.h"
#include "core/error.h"
#include "core/thermostat.h"   // thermostat_state_t

#include <stdio.h>

static const char *TAG = "DRV_DISPLAY";

/**
 * @brief Initialize the display subsystem.
 *
 * For now this is a stub that only logs. Later this is where you:
 *  - init I2C or SPI for the LCD
 *  - reset the display
 *  - configure contrast, backlight, etc.
 */
app_error_t drv_display_init(void)
{
    log_post(LOG_LEVEL_INFO, TAG, "Display init stub (no real LCD yet)");
    return ERR_OK;
}

/**
 * @brief Render the given thermostat state on the UI.
 *
 * Currently implemented as a structured log only. Once the LCD driver
 * is ready, this function is the single place where you:
 *
 *   - clear the display
 *   - print Tin / Tout / setpoint / mode / output state
 *
 * so that the rest of the system stays independent of UI details.
 */
app_error_t drv_display_show_state(const thermostat_state_t *state)
{
    if (state == NULL) {
        return ERR_GENERIC;
    }

    // In the future, replace this block with real LCD code.
    // Example for a 16x2 character LCD:
    //
    // lcd_clear();
    // char line1[17];
    // char line2[17];
    // snprintf(line1, sizeof(line1), "Tin:%4.1fC Sp:%4.1fC",
    //          state->tin_c, state->setpoint_c);
    // snprintf(line2, sizeof(line2), "Tout:%4.1fC %s",
    //          state->tout_c,
    //          (state->output == THERMOSTAT_OUTPUT_HEAT_ON) ? "HEAT" : "IDLE");
    // lcd_set_cursor(0, 0);
    // lcd_print(line1);
    // lcd_set_cursor(1, 0);
    // lcd_print(line2);

    // For now: log the same info so we can see it in the console.
    const char *out_str =
        (state->output == THERMOSTAT_OUTPUT_HEAT_ON) ? "HEAT_ON" : "HEAT_OFF";

    log_post(LOG_LEVEL_INFO, TAG,
             "UI: Tin=%.2fC Tout=%.2fC Sp=%.2fC Hyst=%.2fC Out=%s",
             state->tin_c,
             state->tout_c,
             state->setpoint_c,
             state->hysteresis_c,
             out_str);

    return ERR_OK;
}
