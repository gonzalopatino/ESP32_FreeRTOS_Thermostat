/**
 * @file provisioning.h
 * @brief Provisioning state and credential storage for first-time setup.
 *
 * This module manages:
 *   - Checking if device has been provisioned (first boot detection)
 *   - Storing/retrieving WiFi credentials (SSID, password)
 *   - Storing/retrieving device API key
 *   - Factory reset (clearing all provisioning data)
 *
 * All data is stored in NVS (Non-Volatile Storage).
 */

#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <stdbool.h>
#include <stddef.h>
#include "core/error.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define PROV_WIFI_SSID_MAX_LEN      32   // Max SSID length per WiFi spec
#define PROV_WIFI_PASS_MAX_LEN      64   // Max WPA2 password length
#define PROV_API_KEY_MAX_LEN        128  // API key from backend
#define PROV_USER_NAME_MAX_LEN      64   // Optional user full name
#define PROV_USER_EMAIL_MAX_LEN     128  // Optional user email
#define PROV_USER_PHONE_MAX_LEN     20   // Optional user phone

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

/**
 * @brief WiFi credentials stored in NVS.
 */
typedef struct {
    char ssid[PROV_WIFI_SSID_MAX_LEN + 1];
    char password[PROV_WIFI_PASS_MAX_LEN + 1];
} prov_wifi_creds_t;

/**
 * @brief User information collected during setup (optional).
 */
typedef struct {
    char full_name[PROV_USER_NAME_MAX_LEN + 1];
    char email[PROV_USER_EMAIL_MAX_LEN + 1];
    char phone[PROV_USER_PHONE_MAX_LEN + 1];
} prov_user_info_t;

// -----------------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------------

/**
 * @brief Initialize the provisioning subsystem.
 *
 * Must be called early in app_main, before any other provisioning functions.
 * Initializes NVS if not already done.
 *
 * @return APP_ERR_OK on success, error code on failure.
 */
app_error_t provisioning_init(void);

// -----------------------------------------------------------------------------
// Provisioning State
// -----------------------------------------------------------------------------

/**
 * @brief Check if device has been provisioned.
 *
 * @return true if device has valid provisioning data, false otherwise.
 */
bool provisioning_is_provisioned(void);

/**
 * @brief Mark device as provisioned.
 *
 * Called after successful first-time setup completion.
 *
 * @return APP_ERR_OK on success.
 */
app_error_t provisioning_mark_complete(void);

/**
 * @brief Factory reset - clear all provisioning data.
 *
 * Erases WiFi credentials, API key, user info, and provisioning flag.
 * Device will enter first-time setup on next boot.
 *
 * @return APP_ERR_OK on success.
 */
app_error_t provisioning_factory_reset(void);

// -----------------------------------------------------------------------------
// WiFi Credentials
// -----------------------------------------------------------------------------

/**
 * @brief Store WiFi credentials in NVS.
 *
 * @param[in] creds  Pointer to credentials structure.
 * @return APP_ERR_OK on success.
 */
app_error_t provisioning_set_wifi_creds(const prov_wifi_creds_t *creds);

/**
 * @brief Retrieve WiFi credentials from NVS.
 *
 * @param[out] creds  Pointer to structure to fill.
 * @return APP_ERR_OK on success, ERR_GENERIC if not found.
 */
app_error_t provisioning_get_wifi_creds(prov_wifi_creds_t *creds);

// -----------------------------------------------------------------------------
// API Key
// -----------------------------------------------------------------------------

/**
 * @brief Store API key in NVS.
 *
 * @param[in] api_key  Null-terminated API key string.
 * @return APP_ERR_OK on success.
 */
app_error_t provisioning_set_api_key(const char *api_key);

/**
 * @brief Retrieve API key from NVS.
 *
 * @param[out] api_key  Buffer to store the key.
 * @param[in]  max_len  Size of the buffer.
 * @return APP_ERR_OK on success, ERR_GENERIC if not found.
 */
app_error_t provisioning_get_api_key(char *api_key, size_t max_len);

// -----------------------------------------------------------------------------
// User Info (Optional)
// -----------------------------------------------------------------------------

/**
 * @brief Store user information in NVS.
 *
 * @param[in] info  Pointer to user info structure.
 * @return APP_ERR_OK on success.
 */
app_error_t provisioning_set_user_info(const prov_user_info_t *info);

/**
 * @brief Retrieve user information from NVS.
 *
 * @param[out] info  Pointer to structure to fill.
 * @return APP_ERR_OK on success, ERR_GENERIC if not found.
 */
app_error_t provisioning_get_user_info(prov_user_info_t *info);

#ifdef __cplusplus
}
#endif

#endif  // PROVISIONING_H
