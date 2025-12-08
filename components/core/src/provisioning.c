/**
 * @file provisioning.c
 * @brief Implementation of provisioning state and credential storage.
 *
 * Uses ESP-IDF NVS (Non-Volatile Storage) to persist:
 *   - Provisioning complete flag
 *   - WiFi SSID and password
 *   - Device API key
 *   - Optional user information
 */

#include "core/provisioning.h"
#include "core/logging.h"
#include "core/error.h"

#include "nvs_flash.h"
#include "nvs.h"

#include <string.h>

// -----------------------------------------------------------------------------
// Private Constants
// -----------------------------------------------------------------------------

static const char *TAG = "PROV";

// NVS namespace for all provisioning data
#define NVS_NAMESPACE       "prov"

// NVS keys
#define KEY_PROVISIONED     "is_prov"      // uint8_t: 1 = provisioned
#define KEY_WIFI_SSID       "wifi_ssid"    // string
#define KEY_WIFI_PASS       "wifi_pass"    // string
#define KEY_API_KEY         "api_key"      // string
#define KEY_USER_NAME       "user_name"    // string
#define KEY_USER_EMAIL      "user_email"   // string
#define KEY_USER_PHONE      "user_phone"   // string

// Magic value to confirm provisioning is complete
#define PROVISIONED_MAGIC   0xAB

// -----------------------------------------------------------------------------
// Private State
// -----------------------------------------------------------------------------

static bool s_initialized = false;

// -----------------------------------------------------------------------------
// Private Helpers
// -----------------------------------------------------------------------------

/**
 * @brief Open NVS handle for provisioning namespace.
 *
 * @param[out] handle  NVS handle to fill.
 * @param[in]  mode    NVS_READONLY or NVS_READWRITE.
 * @return ESP_OK on success.
 */
static esp_err_t nvs_open_prov(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    esp_err_t err = nvs_open(NVS_NAMESPACE, mode, handle);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        // Only log actual errors, NOT_FOUND is expected on first boot
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to open NVS namespace '%s': %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
    }
    return err;
}

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

app_error_t provisioning_init(void)
{
    if (s_initialized) {
        return APP_ERR_OK;
    }

    // Initialize NVS flash if not already done
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        log_post(LOG_LEVEL_WARN, TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "NVS flash init failed: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    s_initialized = true;
    log_post(LOG_LEVEL_INFO, TAG, "Provisioning subsystem initialized");

    return APP_ERR_OK;
}

bool provisioning_is_provisioned(void)
{
    if (!s_initialized) {
        log_post(LOG_LEVEL_ERROR, TAG, "Not initialized");
        return false;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READONLY) != ESP_OK) {
        return false;
    }

    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, KEY_PROVISIONED, &value);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        log_post(LOG_LEVEL_INFO, TAG, "Provisioning flag not found - first boot");
        return false;
    }

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Error reading provisioning flag: %s", esp_err_to_name(err));
        return false;
    }

    bool is_prov = (value == PROVISIONED_MAGIC);
    log_post(LOG_LEVEL_DEBUG, TAG, "Provisioning check: %s",
             is_prov ? "PROVISIONED" : "NOT PROVISIONED");

    return is_prov;
}

app_error_t provisioning_mark_complete(void)
{
    if (!s_initialized) {
        return ERR_GENERIC;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READWRITE) != ESP_OK) {
        return ERR_GENERIC;
    }

    esp_err_t err = nvs_set_u8(handle, KEY_PROVISIONED, PROVISIONED_MAGIC);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to mark provisioned: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Device marked as PROVISIONED");
    return APP_ERR_OK;
}

app_error_t provisioning_factory_reset(void)
{
    if (!s_initialized) {
        return ERR_GENERIC;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READWRITE) != ESP_OK) {
        return ERR_GENERIC;
    }

    // Erase all keys in the namespace
    esp_err_t err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Factory reset failed: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_WARN, TAG, "FACTORY RESET complete - all provisioning data erased");
    return APP_ERR_OK;
}

// -----------------------------------------------------------------------------
// WiFi Credentials
// -----------------------------------------------------------------------------

app_error_t provisioning_set_wifi_creds(const prov_wifi_creds_t *creds)
{
    if (!s_initialized || creds == NULL) {
        return ERR_GENERIC;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READWRITE) != ESP_OK) {
        return ERR_GENERIC;
    }

    esp_err_t err = nvs_set_str(handle, KEY_WIFI_SSID, creds->ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, KEY_WIFI_PASS, creds->password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to store WiFi creds: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "WiFi credentials stored (SSID=%s)", creds->ssid);
    return APP_ERR_OK;
}

app_error_t provisioning_get_wifi_creds(prov_wifi_creds_t *creds)
{
    if (!s_initialized || creds == NULL) {
        return ERR_GENERIC;
    }

    memset(creds, 0, sizeof(*creds));

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READONLY) != ESP_OK) {
        return ERR_GENERIC;
    }

    size_t ssid_len = sizeof(creds->ssid);
    size_t pass_len = sizeof(creds->password);

    esp_err_t err = nvs_get_str(handle, KEY_WIFI_SSID, creds->ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, KEY_WIFI_PASS, creds->password, &pass_len);
    }
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        log_post(LOG_LEVEL_DEBUG, TAG, "WiFi credentials not found in NVS");
        return ERR_GENERIC;
    }

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to read WiFi creds: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "WiFi credentials loaded (SSID=%s)", creds->ssid);
    return APP_ERR_OK;
}

// -----------------------------------------------------------------------------
// API Key
// -----------------------------------------------------------------------------

app_error_t provisioning_set_api_key(const char *api_key)
{
    if (!s_initialized || api_key == NULL) {
        return ERR_GENERIC;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READWRITE) != ESP_OK) {
        return ERR_GENERIC;
    }

    esp_err_t err = nvs_set_str(handle, KEY_API_KEY, api_key);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to store API key: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "API key stored (len=%d)", strlen(api_key));
    return APP_ERR_OK;
}

app_error_t provisioning_get_api_key(char *api_key, size_t max_len)
{
    if (!s_initialized || api_key == NULL || max_len == 0) {
        return ERR_GENERIC;
    }

    memset(api_key, 0, max_len);

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READONLY) != ESP_OK) {
        return ERR_GENERIC;
    }

    size_t len = max_len;
    esp_err_t err = nvs_get_str(handle, KEY_API_KEY, api_key, &len);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        log_post(LOG_LEVEL_DEBUG, TAG, "API key not found in NVS");
        return ERR_GENERIC;
    }

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to read API key: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "API key loaded (len=%d)", strlen(api_key));
    return APP_ERR_OK;
}

// -----------------------------------------------------------------------------
// User Info (Optional)
// -----------------------------------------------------------------------------

app_error_t provisioning_set_user_info(const prov_user_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ERR_GENERIC;
    }

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READWRITE) != ESP_OK) {
        return ERR_GENERIC;
    }

    esp_err_t err = ESP_OK;

    if (strlen(info->full_name) > 0) {
        err = nvs_set_str(handle, KEY_USER_NAME, info->full_name);
    }
    if (err == ESP_OK && strlen(info->email) > 0) {
        err = nvs_set_str(handle, KEY_USER_EMAIL, info->email);
    }
    if (err == ESP_OK && strlen(info->phone) > 0) {
        err = nvs_set_str(handle, KEY_USER_PHONE, info->phone);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Failed to store user info: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "User info stored");
    return APP_ERR_OK;
}

app_error_t provisioning_get_user_info(prov_user_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ERR_GENERIC;
    }

    memset(info, 0, sizeof(*info));

    nvs_handle_t handle;
    if (nvs_open_prov(&handle, NVS_READONLY) != ESP_OK) {
        return ERR_GENERIC;
    }

    size_t name_len = sizeof(info->full_name);
    size_t email_len = sizeof(info->email);
    size_t phone_len = sizeof(info->phone);

    // These may not exist, so we don't fail on NOT_FOUND
    nvs_get_str(handle, KEY_USER_NAME, info->full_name, &name_len);
    nvs_get_str(handle, KEY_USER_EMAIL, info->email, &email_len);
    nvs_get_str(handle, KEY_USER_PHONE, info->phone, &phone_len);

    nvs_close(handle);

    log_post(LOG_LEVEL_DEBUG, TAG, "User info loaded");
    return APP_ERR_OK;
}
