#ifndef DRV_DISPLAY_H
#define DRV_DISPLAY_H

#include "core/app_types.h"   // app_error_t
#include "core/thermostat.h"  // thermostat_state_t

/**
 * @brief Initialize display hardware / subsystem.
 */
app_error_t drv_display_init(void);

/**
 * @brief Show the given thermostat state on the display.
 */
app_error_t drv_display_show_state(const thermostat_state_t *state);

#endif // DRV_DISPLAY_H
