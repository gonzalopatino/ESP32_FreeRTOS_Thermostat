#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/error.h"
#include "core/watchdog.h"

#include "app/task_net.h"

static const char *TAG = "NET";

static int s_retry_count = 0;

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
        // We could parse IP here, but logging "got IP" is enough for now
        log_post(LOG_LEVEL_INFO, TAG, "Wi-Fi connected, got IP address");
        s_retry_count = 0;
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
        error_fatal(ERR_GENERIC, "nvs_flash_init");
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
    while (1) {
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
