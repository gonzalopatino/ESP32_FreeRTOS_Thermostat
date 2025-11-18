#ifndef DRV_DISPLAY_H
#define DRV_DISPLAY_H

#include <stdint.h>

#include "core/error.h"        // app_error_t
#include "core/thermostat.h"   // thermostat_state_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file drv_display.h
 * @brief HD44780 16x2 LCD driver abstraction.
 *
 * This module exposes a simple, task safe API for the DISPLAY task:
 *   - initialize the LCD
 *   - clear the screen
 *   - write one logical line at a time (always 16 chars, padded)
 *   - render a high level thermostat_state_t snapshot
 */

#define LCD_ROWS 2
#define LCD_COLS 16

/**
 * @brief Initialize the LCD hardware and put it in 4 bit, 2 line mode.
 *
 * Safe to call once at startup from the DISPLAY task.
 */
app_error_t drv_display_init(void);

/**
 * @brief Clear the entire display and reset the cursor to home.
 */
app_error_t drv_display_clear(void);

/**
 * @brief Write a single logical line to the LCD.
 *
 * This function:
 *   - truncates the input string to LCD_COLS characters if it is longer
 *   - pads with spaces up to LCD_COLS characters if it is shorter
 *   - always writes exactly LCD_COLS characters to the given row
 *
 * @param row  Zero based row index (0 or 1 on a 16x2 display).
 * @param text Null terminated C string to render.
 */
app_error_t drv_display_write_line(uint8_t row, const char *text);

/**
 * @brief High level helper to render a thermostat_state_t snapshot.
 *
 * Typical layout:
 *   Line 0: indoor and outdoor temperatures
 *   Line 1: setpoint, hysteresis, and heater output state
 *
 * The exact formatting lives in the driver and can evolve without
 * touching the tasks that call it.
 *
 * @param state Pointer to current thermostat state. Must not be NULL.
 */
app_error_t drv_display_show_state(const thermostat_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DRV_DISPLAY_H */
