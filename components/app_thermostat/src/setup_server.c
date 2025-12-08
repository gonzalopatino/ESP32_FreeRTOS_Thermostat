/**
 * @file setup_server.c
 * @brief Setup mode server implementation.
 *
 * Implements SoftAP WiFi and HTTP server for first-time device setup.
 */

#include "app/setup_server.h"
#include "core/config.h"
#include "core/logging.h"
#include "core/provisioning.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#include <string.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Private Constants & State
// -----------------------------------------------------------------------------

static const char *TAG = "SETUP_SRV";

// Event bits for WiFi events
#define WIFI_AP_STARTED_BIT   BIT0

static EventGroupHandle_t s_wifi_event_group = NULL;
static httpd_handle_t s_http_server = NULL;
static bool s_server_running = false;
static bool s_setup_complete = false;
static char s_ap_ssid[32] = {0};

// -----------------------------------------------------------------------------
// HTML Pages (embedded)
// -----------------------------------------------------------------------------

static const char *HTML_LANDING_PAGE = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>ThinkSense Thermostat Setup</title>"
"<style>"
"* { box-sizing: border-box; margin: 0; padding: 0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
"       background: linear-gradient(135deg, #1e3a5f 0%, #2d5a87 100%); "
"       min-height: 100vh; display: flex; align-items: center; justify-content: center; "
"       padding: 20px; }"
".container { background: white; border-radius: 16px; padding: 40px; "
"             max-width: 400px; width: 100%; box-shadow: 0 20px 60px rgba(0,0,0,0.3); }"
".logo { text-align: center; margin-bottom: 30px; }"
".logo h1 { color: #1e3a5f; font-size: 24px; margin-bottom: 8px; }"
".logo p { color: #666; font-size: 14px; }"
".step { background: #f8f9fa; border-radius: 12px; padding: 20px; margin-bottom: 20px; }"
".step-header { display: flex; align-items: center; margin-bottom: 12px; }"
".step-num { background: #1e3a5f; color: white; width: 28px; height: 28px; "
"            border-radius: 50%; display: flex; align-items: center; justify-content: center; "
"            font-weight: bold; font-size: 14px; margin-right: 12px; }"
".step h3 { color: #333; font-size: 16px; }"
".step p { color: #666; font-size: 14px; line-height: 1.5; }"
".btn { display: block; width: 100%; padding: 16px; background: #1e3a5f; color: white; "
"       border: none; border-radius: 8px; font-size: 16px; font-weight: 600; "
"       cursor: pointer; text-decoration: none; text-align: center; margin-top: 20px; }"
".btn:hover { background: #2d5a87; }"
".footer { text-align: center; margin-top: 20px; color: #999; font-size: 12px; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<div class=\"logo\">"
"<h1>ThinkSense Thermostat</h1>"
"<p>First-Time Setup Wizard</p>"
"</div>"
"<div class=\"step\">"
"<div class=\"step-header\"><div class=\"step-num\">1</div><h3>Enter API Key</h3></div>"
"<p>Enter the API key you received when registering your device online.</p>"
"</div>"
"<div class=\"step\">"
"<div class=\"step-header\"><div class=\"step-num\">2</div><h3>Connect to WiFi</h3></div>"
"<p>Select your home WiFi network and enter the password.</p>"
"</div>"
"<div class=\"step\">"
"<div class=\"step-header\"><div class=\"step-num\">3</div><h3>Your Information</h3></div>"
"<p>Optionally provide your contact details for alerts.</p>"
"</div>"
"<a href=\"/setup/step1\" class=\"btn\">Begin Setup</a>"
"<div class=\"footer\">ThinkSense IoT &copy; 2025</div>"
"</div>"
"</body>"
"</html>";

static const char *HTML_STEP1_PAGE =
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>Step 1: API Key</title>"
"<style>"
"* { box-sizing: border-box; margin: 0; padding: 0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
"       background: linear-gradient(135deg, #1e3a5f 0%, #2d5a87 100%); "
"       min-height: 100vh; display: flex; align-items: center; justify-content: center; "
"       padding: 20px; }"
".container { background: white; border-radius: 16px; padding: 40px; "
"             max-width: 400px; width: 100%; box-shadow: 0 20px 60px rgba(0,0,0,0.3); }"
"h1 { color: #1e3a5f; font-size: 22px; margin-bottom: 8px; }"
".subtitle { color: #666; font-size: 14px; margin-bottom: 24px; }"
".progress { display: flex; margin-bottom: 30px; }"
".progress-step { flex: 1; height: 4px; background: #e0e0e0; margin-right: 8px; border-radius: 2px; }"
".progress-step.active { background: #1e3a5f; }"
".progress-step:last-child { margin-right: 0; }"
"label { display: block; color: #333; font-weight: 500; margin-bottom: 8px; font-size: 14px; }"
"input[type=text] { width: 100%; padding: 14px; border: 2px solid #e0e0e0; border-radius: 8px; "
"                   font-size: 16px; margin-bottom: 20px; }"
"input[type=text]:focus { outline: none; border-color: #1e3a5f; }"
".btn { display: block; width: 100%; padding: 16px; background: #1e3a5f; color: white; "
"       border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; }"
".btn:hover { background: #2d5a87; }"
".info { background: #e3f2fd; border-radius: 8px; padding: 12px; margin-bottom: 20px; "
"        font-size: 13px; color: #1565c0; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<div class=\"progress\">"
"<div class=\"progress-step active\"></div>"
"<div class=\"progress-step\"></div>"
"<div class=\"progress-step\"></div>"
"</div>"
"<h1>Step 1: API Key</h1>"
"<p class=\"subtitle\">Enter the API key from your device registration</p>"
"<div class=\"info\">You can find your API key in the ThinkSense dashboard after registering your device serial number.</div>"
"<form action=\"/setup/step1\" method=\"POST\">"
"<label for=\"api_key\">API Key</label>"
"<input type=\"text\" id=\"api_key\" name=\"api_key\" placeholder=\"Enter your API key\" required>"
"<button type=\"submit\" class=\"btn\">Next Step</button>"
"</form>"
"</div>"
"</body>"
"</html>";

// -----------------------------------------------------------------------------
// WiFi Event Handler
// -----------------------------------------------------------------------------

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                log_post(LOG_LEVEL_INFO, TAG, "SoftAP started: %s", s_ap_ssid);
                xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
                break;

            case WIFI_EVENT_AP_STOP:
                log_post(LOG_LEVEL_INFO, TAG, "SoftAP stopped");
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                log_post(LOG_LEVEL_INFO, TAG, "Client connected, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5]);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                log_post(LOG_LEVEL_INFO, TAG, "Client disconnected, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5]);
                break;
            }

            default:
                break;
        }
    }
}

// -----------------------------------------------------------------------------
// HTTP Handlers
// -----------------------------------------------------------------------------

static esp_err_t handler_root(httpd_req_t *req)
{
    log_post(LOG_LEVEL_DEBUG, TAG, "HTTP GET /");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_LANDING_PAGE, strlen(HTML_LANDING_PAGE));
    return ESP_OK;
}

static esp_err_t handler_step1_get(httpd_req_t *req)
{
    log_post(LOG_LEVEL_DEBUG, TAG, "HTTP GET /setup/step1");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_STEP1_PAGE, strlen(HTML_STEP1_PAGE));
    return ESP_OK;
}

static esp_err_t handler_step1_post(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "HTTP POST /setup/step1");

    // Read POST body
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "POST body: %s", buf);

    // Parse api_key from form data (api_key=xxx)
    char *api_key_start = strstr(buf, "api_key=");
    if (api_key_start) {
        api_key_start += strlen("api_key=");
        
        // Find end of value
        char *api_key_end = strchr(api_key_start, '&');
        if (api_key_end) {
            *api_key_end = '\0';
        }

        // URL decode would go here in production
        log_post(LOG_LEVEL_INFO, TAG, "Received API key (len=%d)", strlen(api_key_start));

        // Store in NVS
        if (provisioning_set_api_key(api_key_start) == APP_ERR_OK) {
            log_post(LOG_LEVEL_INFO, TAG, "API key saved to NVS");
        } else {
            log_post(LOG_LEVEL_ERROR, TAG, "Failed to save API key");
        }
    }

    // Redirect to step 2 (will be implemented in next increment)
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/setup/step2");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t handler_step2_get(httpd_req_t *req)
{
    // Placeholder for WiFi setup page
    const char *html = 
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>Step 2: WiFi</title></head><body style=\"font-family: sans-serif; padding: 20px;\">"
        "<h1>Step 2: WiFi Setup</h1>"
        "<p>WiFi configuration coming in next increment...</p>"
        "<a href=\"/\">Back to Start</a>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// SoftAP Initialization
// -----------------------------------------------------------------------------

static app_error_t init_softap(void)
{
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to create event group");
        return ERR_GENERIC;
    }

    // Initialize NVS (may already be done)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default AP network interface
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Get MAC address for SSID suffix
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s%02X%02X",
             SETUP_AP_SSID_PREFIX, mac[4], mac[5]);

    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(s_ap_ssid),
            .channel = SETUP_AP_CHANNEL,
            .max_connection = SETUP_AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,  // Open network for easy setup
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    memcpy(wifi_config.ap.ssid, s_ap_ssid, strlen(s_ap_ssid));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for AP to start
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_AP_STARTED_BIT,
                                            pdFALSE, pdTRUE,
                                            pdMS_TO_TICKS(10000));

    if (!(bits & WIFI_AP_STARTED_BIT)) {
        log_post(LOG_LEVEL_ERROR, TAG, "SoftAP failed to start");
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "SoftAP ready: SSID=%s, IP=192.168.4.1", s_ap_ssid);
    return APP_ERR_OK;
}

// -----------------------------------------------------------------------------
// HTTP Server Initialization
// -----------------------------------------------------------------------------

static app_error_t init_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    log_post(LOG_LEVEL_INFO, TAG, "Starting HTTP server on port %d", config.server_port);

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    // Register URI handlers
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_root,
    };
    httpd_register_uri_handler(s_http_server, &uri_root);

    httpd_uri_t uri_step1_get = {
        .uri = "/setup/step1",
        .method = HTTP_GET,
        .handler = handler_step1_get,
    };
    httpd_register_uri_handler(s_http_server, &uri_step1_get);

    httpd_uri_t uri_step1_post = {
        .uri = "/setup/step1",
        .method = HTTP_POST,
        .handler = handler_step1_post,
    };
    httpd_register_uri_handler(s_http_server, &uri_step1_post);

    httpd_uri_t uri_step2_get = {
        .uri = "/setup/step2",
        .method = HTTP_GET,
        .handler = handler_step2_get,
    };
    httpd_register_uri_handler(s_http_server, &uri_step2_get);

    log_post(LOG_LEVEL_INFO, TAG, "HTTP server started with %d endpoints", 4);
    return APP_ERR_OK;
}

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

app_error_t setup_server_start(void)
{
    if (s_server_running) {
        log_post(LOG_LEVEL_WARN, TAG, "Setup server already running");
        return APP_ERR_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Starting setup server...");

    // Initialize SoftAP
    app_error_t err = init_softap();
    if (err != APP_ERR_OK) {
        return err;
    }

    // Initialize HTTP server
    err = init_http_server();
    if (err != APP_ERR_OK) {
        return err;
    }

    s_server_running = true;
    log_post(LOG_LEVEL_INFO, TAG, "Setup server ready!");

    return APP_ERR_OK;
}

app_error_t setup_server_stop(void)
{
    if (!s_server_running) {
        return APP_ERR_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Stopping setup server...");

    // Stop HTTP server
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    s_server_running = false;
    log_post(LOG_LEVEL_INFO, TAG, "Setup server stopped");

    return APP_ERR_OK;
}

bool setup_server_is_running(void)
{
    return s_server_running;
}

app_error_t setup_server_get_ssid(char *ssid_buf, size_t buf_len)
{
    if (!ssid_buf || buf_len < strlen(s_ap_ssid) + 1) {
        return ERR_GENERIC;
    }

    strncpy(ssid_buf, s_ap_ssid, buf_len);
    return APP_ERR_OK;
}

bool setup_server_is_complete(void)
{
    return s_setup_complete;
}
