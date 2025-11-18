#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <string.h>
#include "core/config.h"
#include "core/logging.h"
#include "esp_system.h"




#include "core/watchdog.h"
#include "esp_http_client.h"
#include "app/task_net.h"

static const char *TAG = "NET";

static int s_retry_count = 0;
static bool s_wifi_ready    = false;   // NEW: got IP
static bool s_sent_telemetry = false;  // NEW: only send once

static void net_send_test_telemetry(void)
{
    // Use the exact URL that worked in Postman
    const char *url =
        "https://gonzaping-e2b5g2fuavc0bcbn.canadacentral-01.azurewebsites.net/api/telemetry";

    const char *json_body =
        "{"
        "\"device_id\":\"esp32-thermostat\","
        "\"tin_c\":27.8,"
        "\"setpoint_c\":22"
        "}";

    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        // .cert_pem = NULL  // use IDF's default cert bundle
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        log_post(LOG_LEVEL_ERROR, TAG, "esp_http_client_init failed");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len    = esp_http_client_get_content_length(client);
        log_post(LOG_LEVEL_INFO, TAG,
                 "Telemetry POST done, status=%d, content_length=%d",
                 status, len);
    } else {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "Telemetry POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}


/**
 * @brief Common Wi-Fi event handler (WIFI_EVENT + IP_EVENT).
 */
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
            log_post(LOG_LEVEL_WARN, TAG, "Wi-Fi disconnected, retrying (%d/%d)",
                     s_retry_count + 1, WIFI_MAX_RETRY);
            esp_wifi_connect();
            s_retry_count++;
        } else {
            log_post(LOG_LEVEL_ERROR, TAG, "Wi-Fi failed to connect after %d retries", WIFI_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi connected, got IP address");
        s_retry_count    = 0;
        s_wifi_ready     = true;   // NEW
        s_sent_telemetry = false;  // allow send once per connection
    }
}


/**
 * @brief Initialize NVS (required by Wi-Fi stack).
 */
static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        // Treat this as unrecoverable - no NVS means no Wi-Fi config storage.
        log_post(LOG_LEVEL_ERROR, TAG,
                 "nvs_flash_init failed: %s", esp_err_to_name(err));
        esp_system_abort("nvs_flash_init failed");
    }
}



/**
 * @brief NET task: bring up Wi-Fi station and keep it running.
 *
 * For now, once Wi-Fi is started we just sleep forever and let the
 * event handler deal with reconnects.
 */
static void task_net(void *arg)
{
    (void)arg;

    watchdog_register_current("NET");

    init_nvs();

    // Initialize underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    (void)netif; // Not used directly, but must exist

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for Wi-Fi and IP events
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

    wifi_config_t wifi_config = {
        .sta = {
            // SSID and password are zero-terminated inside the struct
        }
    };

    // Copy SSID and password safely
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASS);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi STA init finished, waiting for connection...");

    // NET task does not need to do much itself; event handler manages reconnects.
        log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi STA init finished, waiting for connection...");

    // NET task does not need to do much itself; event handler manages reconnects.
    while (1) {
        if (s_wifi_ready && !s_sent_telemetry) {
            log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi ready, sending test telemetry...");
            net_send_test_telemetry();
            s_sent_telemetry = true;
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
