/**
 * @file boot_mode.h
 * @brief Boot mode state machine for normal vs setup mode operation.
 *
 * The thermostat supports two boot paths:
 *   - NORMAL: Device is provisioned, runs thermostat tasks
 *   - SETUP:  Device needs provisioning, runs softAP + HTTP wizard
 *
 * Setup mode is entered when:
 *   1. Device has never been provisioned (first boot)
 *   2. User holds MODE button for 3+ seconds at boot
 *   3. User triggers factory reset
 */

#ifndef BOOT_MODE_H
#define BOOT_MODE_H

#include <stdbool.h>
#include <stdint.h>
#include "core/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Boot Mode Enum
// -----------------------------------------------------------------------------

/**
 * @brief Boot mode states for the thermostat.
 */
typedef enum {
    BOOT_MODE_UNKNOWN = 0,  // Initial state before determination
    BOOT_MODE_NORMAL,       // Provisioned - run thermostat tasks
    BOOT_MODE_SETUP,        // Not provisioned - run setup wizard
} boot_mode_t;

// -----------------------------------------------------------------------------
// API Functions
// -----------------------------------------------------------------------------

/**
 * @brief Determine the boot mode based on provisioning state.
 *
 * Checks NVS provisioning flag and returns appropriate mode.
 * Must be called after provisioning_init().
 *
 * @return BOOT_MODE_NORMAL if provisioned, BOOT_MODE_SETUP otherwise.
 */
boot_mode_t boot_mode_determine(void);

/**
 * @brief Get the current boot mode.
 *
 * @return Current boot mode (set by boot_mode_determine or boot_mode_force_setup).
 */
boot_mode_t boot_mode_get(void);

/**
 * @brief Force entry into setup mode.
 *
 * Used when user holds MODE button for 3+ seconds.
 * Does NOT clear provisioning data - that's done by provisioning_factory_reset().
 */
void boot_mode_force_setup(void);

/**
 * @brief Get human-readable string for boot mode.
 *
 * @param mode  Boot mode enum value.
 * @return Static string like "NORMAL", "SETUP", or "UNKNOWN".
 */
const char *boot_mode_to_str(boot_mode_t mode);

/**
 * @brief Check if MODE button is held at boot (for manual setup entry).
 *
 * Call this early in boot. If button is held for the specified duration,
 * returns true to indicate user wants to enter setup mode.
 *
 * @param hold_ms  Duration in milliseconds the button must be held.
 * @return true if button held for required duration, false otherwise.
 */
bool boot_mode_check_button_held(uint32_t hold_ms);

#ifdef __cplusplus
}
#endif

#endif  // BOOT_MODE_H
