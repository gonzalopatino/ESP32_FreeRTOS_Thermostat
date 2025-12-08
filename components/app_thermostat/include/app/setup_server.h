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

/**
 * @brief Get the security PIN displayed on the device LCD.
 *
 * The PIN is randomly generated when the setup server starts.
 * User must enter this PIN in the browser to access the setup wizard.
 *
 * @return 4-digit PIN string (e.g., "1234").
 */
const char* setup_server_get_pin(void);

/**
 * @brief Check if WiFi connection has been requested.
 *
 * Call this periodically from the main loop to detect when
 * the user has submitted WiFi credentials and requested connection.
 *
 * @return true if connection is pending.
 */
bool setup_server_connect_pending(void);

/**
 * @brief Perform the WiFi mode switch from AP to STA.
 *
 * This function:
 *   1. Stops the setup server (HTTP + DNS)
 *   2. Disconnects the SoftAP
 *   3. Connects to the home WiFi using saved credentials
 *
 * @param[out] ip_addr   Buffer to store assigned IP (min 16 bytes), or NULL.
 * @param[in]  buf_len   Size of IP buffer.
 * @return APP_ERR_OK on successful connection, error code on failure.
 */
app_error_t setup_server_connect_wifi(char *ip_addr, size_t buf_len);

/**
 * @brief Start HTTP server on STA mode for Step 3 (API key setup).
 *
 * Call this after setup_server_connect_wifi() succeeds.
 * Starts an HTTP server on the home network to serve the API key entry page.
 *
 * @return APP_ERR_OK on success.
 */
app_error_t setup_server_start_step3(void);

/**
 * @brief Set the device IP address for display in Step 3 page.
 *
 * @param ip  The IP address string (e.g., "10.0.0.166").
 */
void setup_server_set_device_ip(const char *ip);

/**
 * @brief Check if API key setup is complete.
 *
 * @return true if API key has been configured via Step 3.
 */
bool setup_server_api_key_complete(void);

/**
 * @brief Start Settings Mode server (AP+STA).
 *
 * Starts a SoftAP while maintaining STA connection, allowing user
 * to access settings page via browser to change WiFi credentials,
 * API key, or perform factory reset.
 *
 * @param[in] device_ip  Current STA IP address (can be NULL if not connected).
 * @return APP_ERR_OK on success.
 */
app_error_t setup_server_start_settings(const char *device_ip);

/**
 * @brief Stop Settings Mode server and return to normal operation.
 *
 * Stops the settings HTTP server and SoftAP, returns to STA-only mode.
 *
 * @return APP_ERR_OK on success.
 */
app_error_t setup_server_stop_settings(void);

/**
 * @brief Check if settings mode is currently active.
 *
 * @return true if settings server is running.
 */
bool setup_server_is_settings_mode(void);

/**
 * @brief Check if settings were modified and saved.
 *
 * @return true if settings were saved (indicating restart may be needed).
 */
bool setup_server_settings_saved(void);

#ifdef __cplusplus
}
#endif

#endif  // SETUP_SERVER_H
