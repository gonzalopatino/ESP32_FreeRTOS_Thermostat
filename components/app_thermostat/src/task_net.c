#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include <string.h>

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/error.h"
#include "core/timeutil.h"        // timeutil_init_sntp, timeutil_is_time_set, timeutil_get_iso8601
#include "core/provisioning.h"    // provisioned WiFi + API key

#include "app/task_common.h"      // g_q_telemetry_state, thermostat_state_t



static const char *TAG = "NET";
static int       s_retry_count         = 0;
static bool      s_wifi_ready          = false;  // got IP
static TickType_t s_last_telemetry_tick = 0;     // last time we sent telemetry






// Device credentials loaded from NVS (or defaults)
static char s_device_serial[64];
static char s_device_api_key[128];

// --- helpers to stringify enums -----------------------------------------

static const char *mode_to_str(thermostat_mode_t mode)
{
    switch (mode) {
        case THERMOSTAT_MODE_OFF:  return "OFF";
        case THERMOSTAT_MODE_HEAT: return "HEAT";
        case THERMOSTAT_MODE_COOL: return "COOL";
        case THERMOSTAT_MODE_AUTO: return "AUTO";
        default:                   return "UNKNOWN";
    }
}

static const char *output_to_str(thermostat_output_t out)
{
    switch (out) {
        case THERMOSTAT_OUTPUT_OFF:      return "OFF";
        case THERMOSTAT_OUTPUT_HEAT_ON:  return "HEAT_ON";
        case THERMOSTAT_OUTPUT_COOL_ON:  return "COOL_ON";
        default:                         return "UNKNOWN";
    }
}

// --- NVS init ------------------------------------------------------------

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "nvs_flash_init failed: %s", esp_err_to_name(err));
        esp_system_abort("nvs_flash_init failed");
    }
}

// --- Device credentials from provisioning -------------------------------
// Load device credentials from provisioning system (NVS).
// Falls back to compile-time defaults if not provisioned.

static void device_creds_init_from_provisioning(void)
{
    // Try to get API key from provisioning
    char api_key_buf[128] = {0};
    if (provisioning_get_api_key(api_key_buf, sizeof(api_key_buf)) == APP_ERR_OK 
        && strlen(api_key_buf) > 10) {
        strlcpy(s_device_api_key, api_key_buf, sizeof(s_device_api_key));
        log_post(LOG_LEVEL_INFO, TAG, "Using provisioned API key (len=%u)", 
                 (unsigned)strlen(s_device_api_key));
    } else {
        // Fallback to compile-time default
        strlcpy(s_device_api_key, DEVICE_API_KEY_DEFAULT, sizeof(s_device_api_key));
        log_post(LOG_LEVEL_WARN, TAG, "No provisioned API key, using default");
    }

    // Serial is still from compile-time default (could be provisioned later)
    strlcpy(s_device_serial, DEVICE_SERIAL_DEFAULT, sizeof(s_device_serial));
    
    log_post(LOG_LEVEL_INFO, TAG,
             "Device creds loaded: serial=\"%s\" (api_key len=%u)",
             s_device_serial,
             (unsigned)strlen(s_device_api_key));
}


// --- HTTP telemetry sender ----------------------------------------------

// Send one snapshot to the Django server.
static void net_send_telemetry(const thermostat_state_t *state)
{
    if (!timeutil_is_time_set()) {
        log_post(LOG_LEVEL_WARN, TAG, "Time not set yet, skipping telemetry send");
        return;
    }

    // Device local timestamp string
    char ts_buf[40];
    if (!timeutil_get_iso8601(ts_buf, sizeof(ts_buf))) {
        log_post(LOG_LEVEL_WARN, TAG, "Failed to format local time, skipping telemetry");
        return;
    }

    char url[128];
    snprintf(url, sizeof(url),
             "http://%s:%s%s",
             TH_SERVER_HOST,
             TH_SERVER_PORT,
             TH_API_INGEST_PATH);

    // Build JSON body – note: no device_id field, server trusts Authorization header
    char json_body[256];
    int len = snprintf(
        json_body,
        sizeof(json_body),
        "{"
          "\"mode\":\"%s\","
          "\"temp_inside_c\":%.2f,"
          "\"temp_outside_c\":%.2f,"
          "\"setpoint_c\":%.2f,"
          "\"hysteresis_c\":%.2f,"
          "\"output\":\"%s\","
          "\"timestamp\":\"%s\""
        "}",
        mode_to_str(state->mode),
        state->tin_c,
        state->tout_c,
        state->setpoint_c,
        state->hysteresis_c,
        output_to_str(state->output),
        ts_buf
    );

    if (len <= 0 || len >= (int)sizeof(json_body)) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to build telemetry JSON");
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Sending telemetry to %s", url);
    log_post(LOG_LEVEL_DEBUG, TAG, "Payload: %s", json_body);

    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .transport_type    = HTTP_TRANSPORT_OVER_TCP,  // plain HTTP on LAN
        .crt_bundle_attach = NULL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        log_post(LOG_LEVEL_ERROR, TAG, "HTTP client init failed");
        return;
    }

    // Build Authorization header from NVS-backed values
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header),
         "Device %s:%s", s_device_serial, s_device_api_key);

    log_post(LOG_LEVEL_DEBUG, TAG, "Auth header: %s", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int       status      = esp_http_client_get_status_code(client);
        long long content_len = esp_http_client_get_content_length(client);
        log_post(LOG_LEVEL_INFO, TAG,
                 "Telemetry POST OK, status=%d len=%lld",
                 status, content_len);
    } else {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Telemetry POST failed: %s",
                 esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// --- Wi-Fi event handler -------------------------------------------------

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi STA started, connecting to SSID \"%s\"", WIFI_SSID);
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            log_post(LOG_LEVEL_WARN, TAG,
                     "Wi-Fi disconnected, retrying (%d/%d)",
                     s_retry_count + 1, WIFI_MAX_RETRY);
            esp_wifi_connect();
            s_retry_count++;
        } else {
            log_post(LOG_LEVEL_ERROR, TAG,
                     "Wi-Fi failed to connect after %d retries", WIFI_MAX_RETRY);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi connected, got IP address");
        log_post(LOG_LEVEL_INFO, TAG, "Starting SNTP...");
        timeutil_init_sntp();
        s_retry_count    = 0;
        s_wifi_ready     = true;
        s_last_telemetry_tick  = 0;  // force immediate send once time is set
    }
}

// --- NET task ------------------------------------------------------------

static void task_net(void *arg)
{
    (void)arg;

    watchdog_register_current("NET");

    init_nvs();
    device_creds_init_from_provisioning();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    (void)netif;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL));

    wifi_config_t wifi_config = (wifi_config_t){ 0 };

    // Get WiFi credentials from provisioning (or fall back to defaults)
    prov_wifi_creds_t wifi_creds = {0};
    
    if (provisioning_get_wifi_creds(&wifi_creds) == APP_ERR_OK 
        && strlen(wifi_creds.ssid) > 0) {
        strlcpy((char *)wifi_config.sta.ssid, wifi_creds.ssid,
                sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, wifi_creds.password,
                sizeof(wifi_config.sta.password));
        log_post(LOG_LEVEL_INFO, TAG, "Using provisioned WiFi: SSID=\"%s\"", wifi_creds.ssid);
    } else {
        // Fallback to compile-time defaults
        strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID,
                sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, WIFI_PASS,
                sizeof(wifi_config.sta.password));
        log_post(LOG_LEVEL_WARN, TAG, "No provisioned WiFi, using default SSID=\"%s\"", WIFI_SSID);
    }

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    log_post(LOG_LEVEL_INFO, TAG,
             "Wi-Fi STA init finished, waiting for connection...");

    log_post(LOG_LEVEL_INFO, TAG, "NET server host=%s port=%s path=%s",
             TH_SERVER_HOST, TH_SERVER_PORT, TH_API_INGEST_PATH);

    while (1) {
        if (s_wifi_ready && timeutil_is_time_set()) {

            TickType_t now = xTaskGetTickCount();

            // Has TELEMETRY_PERIOD_MS elapsed since last send?
            if (now - s_last_telemetry_tick >= pdMS_TO_TICKS(TELEMETRY_PERIOD_MS)) {

                thermostat_state_t snap;

                if (!g_q_telemetry_state) {
                    log_post(LOG_LEVEL_ERROR, TAG,
                            "g_q_telemetry_state is NULL, cannot send telemetry");
                } else if (xQueuePeek(g_q_telemetry_state,
                                    &snap,
                                    0) == pdTRUE) {

                    log_post(LOG_LEVEL_INFO, TAG,
                            "Wi-Fi + time ready, sending telemetry...");
                    net_send_telemetry(&snap);
                    s_last_telemetry_tick = now;

                } else {
                    log_post(LOG_LEVEL_WARN, TAG,
                            "No telemetry snapshot available in queue");
                }
            }
        }

        watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(500));  // check twice per second
    }

}

void task_net_start(void)
{
    xTaskCreate(
        task_net,
        "task_net",
        TASK_STACK_NET,
        NULL,
        TASK_PRIO_NET,
        NULL);
}
