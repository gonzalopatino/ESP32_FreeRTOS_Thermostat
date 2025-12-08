/**
 * @file setup_server.h
 * @brief Setup mode server for first-time WiFi provisioning.
 *
 * This module manages:
 *   - SoftAP WiFi access point (ThinkSenseThermo-XXXX)
 *   - HTTP server for the setup wizard
 *   - Endpoints for API key, WiFi credentials, and user info
 */

#ifndef SETUP_SERVER_H
#define SETUP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include "core/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define SETUP_AP_SSID_PREFIX    "ThinkSenseThermo-"
#define SETUP_AP_CHANNEL        1
#define SETUP_AP_MAX_CONN       4
#define SETUP_SERVER_PORT       80

// -----------------------------------------------------------------------------
// API Functions
// -----------------------------------------------------------------------------

/**
 * @brief Start the setup server (SoftAP + HTTP).
 *
 * This function:
 *   1. Initializes WiFi in AP mode
 *   2. Creates SSID using MAC address suffix
 *   3. Starts HTTP server with setup wizard endpoints
 *
 * @return APP_ERR_OK on success, error code on failure.
 */
app_error_t setup_server_start(void);

/**
 * @brief Stop the setup server.
 *
 * Stops HTTP server and disables SoftAP.
 * Call this after provisioning is complete.
 *
 * @return APP_ERR_OK on success.
 */
app_error_t setup_server_stop(void);

/**
 * @brief Check if setup server is running.
 *
 * @return true if server is active, false otherwise.
 */
bool setup_server_is_running(void);

/**
 * @brief Get the SoftAP SSID being broadcast.
 *
 * @param[out] ssid_buf  Buffer to store SSID (min 32 bytes).
 * @param[in]  buf_len   Size of buffer.
 * @return APP_ERR_OK on success.
 */
app_error_t setup_server_get_ssid(char *ssid_buf, size_t buf_len);

/**
 * @brief Check if setup has been completed via the wizard.
 *
 * This flag is set when the user completes all setup steps.
 *
 * @return true if setup completed, false otherwise.
 */
bool setup_server_is_complete(void);

#ifdef __cplusplus
}
#endif

#endif  // SETUP_SERVER_H
