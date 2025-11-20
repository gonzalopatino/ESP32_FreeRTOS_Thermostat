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
#include "core/timeutil.h"        // timeutil_init_sntp, timeutil_is_time_valid, timeutil_get_local_iso8601

#include "app/task_common.h"      // g_q_telemetry_state, thermostat_state_t

static const char *TAG = "NET";

static int  s_retry_count    = 0;
static bool s_wifi_ready     = false;  // got IP
static bool s_sent_telemetry = false;  // only send once per boot



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


    char json_body[256];

    int len = snprintf(
        json_body,
        sizeof(json_body),
        "{"
          "\"device_id\":\"%s\","
          "\"mode\":\"%s\","
          "\"temp_inside_c\":%.2f,"
          "\"temp_outside_c\":%.2f,"
          "\"setpoint_c\":%.2f,"
          "\"hysteresis_c\":%.2f,"
          "\"output\":\"%s\","
          "\"timestamp\":\"%s\""
        "}",
        "esp32-thermostat-1",
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

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-API-Key", TH_SERVER_API_KEY);
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
        s_sent_telemetry = false;
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

// --- NET task ------------------------------------------------------------

static void task_net(void *arg)
{
    (void)arg;

    watchdog_register_current("NET");

    init_nvs();

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

    wifi_config_t wifi_config = { 0 };

    snprintf((char *)wifi_config.sta.ssid,
             sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password,
             sizeof(wifi_config.sta.password), "%s", WIFI_PASS);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    log_post(LOG_LEVEL_INFO, TAG,
             "Wi-Fi STA init finished, waiting for connection...");

    log_post(LOG_LEVEL_INFO, TAG, "NET server host=%s port=%s path=%s",
         TH_SERVER_HOST, TH_SERVER_PORT, TH_API_INGEST_PATH);


    while (1) {
        if (s_wifi_ready && timeutil_is_time_set() && !s_sent_telemetry) {

            thermostat_state_t snap;

            if (!g_q_telemetry_state) {
                log_post(LOG_LEVEL_ERROR, TAG,
                         "g_q_telemetry_state is NULL, cannot send telemetry");
                s_sent_telemetry = true; // avoid spamming
            } else if (xQueueReceive(g_q_telemetry_state,
                                     &snap,
                                     pdMS_TO_TICKS(5000)) == pdTRUE) {

                log_post(LOG_LEVEL_INFO, TAG,
                         "Wi-Fi + time ready, sending telemetry...");
                net_send_telemetry(&snap);
                s_sent_telemetry = true;

            } else {
                log_post(LOG_LEVEL_WARN, TAG,
                         "Timeout waiting for telemetry snapshot");
                // we will try again next loop
            }
        }

        watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
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
