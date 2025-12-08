/**
 * @file setup_server.c
 * @brief Setup mode server implementation with captive portal and PIN security.
 *
 * Implements SoftAP WiFi, captive portal (auto-redirect), and HTTP server 
 * for first-time device setup. Requires PIN entry for security.
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
#include "esp_random.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// -----------------------------------------------------------------------------
// Private Constants & State
// -----------------------------------------------------------------------------

static const char *TAG = "SETUP_SRV";

// Event bits for WiFi events
#define WIFI_AP_STARTED_BIT   BIT0
#define WIFI_CONNECTED_BIT    BIT1

// DNS server constants
#define DNS_PORT              53
#define DNS_MAX_LEN           512

static EventGroupHandle_t s_wifi_event_group = NULL;
static httpd_handle_t s_http_server = NULL;
static bool s_server_running = false;
static bool s_setup_complete = false;
static bool s_pin_verified = false;
static bool s_api_key_complete = false;
static char s_ap_ssid[32] = {0};
static char s_pin[5] = {0};  // 4-digit PIN + null terminator

// DNS server task handle
static TaskHandle_t s_dns_task_handle = NULL;
static int s_dns_socket = -1;

// Flag to signal WiFi connection request
static bool s_connect_pending = false;

// -----------------------------------------------------------------------------
// PIN Generation
// -----------------------------------------------------------------------------

static void generate_pin(void)
{
    uint32_t random = esp_random();
    // Generate 4-digit PIN (1000-9999 to avoid leading zeros)
    uint16_t pin_num = 1000 + (random % 9000);
    snprintf(s_pin, sizeof(s_pin), "%04d", pin_num);
    log_post(LOG_LEVEL_INFO, TAG, "Security PIN generated");
}

// -----------------------------------------------------------------------------
// HTML Pages (embedded)
// -----------------------------------------------------------------------------

// PIN entry page (shown first via captive portal)
static const char *HTML_PIN_PAGE = 
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>ThinkSense Thermostat - Security</title>"
"<style>"
"* { box-sizing: border-box; margin: 0; padding: 0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
"       background: linear-gradient(135deg, #1e3a5f 0%, #2d5a87 100%); "
"       min-height: 100vh; display: flex; align-items: center; justify-content: center; "
"       padding: 20px; }"
".container { background: white; border-radius: 16px; padding: 40px; "
"             max-width: 400px; width: 100%; box-shadow: 0 20px 60px rgba(0,0,0,0.3); "
"             text-align: center; }"
".logo { margin-bottom: 30px; }"
".logo h1 { color: #1e3a5f; font-size: 22px; margin-bottom: 8px; }"
".logo p { color: #666; font-size: 14px; }"
".lock-icon { font-size: 48px; margin-bottom: 20px; }"
".info { background: #fff3cd; border-radius: 8px; padding: 16px; margin-bottom: 24px; "
"        font-size: 14px; color: #856404; text-align: left; }"
".pin-input { display: flex; justify-content: center; gap: 10px; margin-bottom: 24px; }"
".pin-input input { width: 50px; height: 60px; text-align: center; font-size: 24px; "
"                   font-weight: bold; border: 2px solid #e0e0e0; border-radius: 8px; }"
".pin-input input:focus { outline: none; border-color: #1e3a5f; }"
".btn { display: block; width: 100%; padding: 16px; background: #1e3a5f; color: white; "
"       border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; }"
".btn:hover { background: #2d5a87; }"
".error { color: #dc3545; margin-bottom: 16px; font-size: 14px; display: none; }"
".error.show { display: block; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<div class=\"logo\">"
"<div class=\"lock-icon\">&#128274;</div>"
"<h1>ThinkSense Thermostat</h1>"
"<p>Security Verification Required</p>"
"</div>"
"<div class=\"info\">"
"<strong>Look at your thermostat display</strong><br>"
"Enter the 4-digit PIN shown on the device screen."
"</div>"
"<div id=\"error\" class=\"error\">Invalid PIN. Please try again.</div>"
"<form action=\"/verify-pin\" method=\"POST\" id=\"pinForm\">"
"<div class=\"pin-input\">"
"<input type=\"text\" maxlength=\"1\" pattern=\"[0-9]\" inputmode=\"numeric\" name=\"d1\" autofocus required>"
"<input type=\"text\" maxlength=\"1\" pattern=\"[0-9]\" inputmode=\"numeric\" name=\"d2\" required>"
"<input type=\"text\" maxlength=\"1\" pattern=\"[0-9]\" inputmode=\"numeric\" name=\"d3\" required>"
"<input type=\"text\" maxlength=\"1\" pattern=\"[0-9]\" inputmode=\"numeric\" name=\"d4\" required>"
"</div>"
"<input type=\"hidden\" name=\"pin\" id=\"pinValue\">"
"<button type=\"submit\" class=\"btn\">Verify PIN</button>"
"</form>"
"</div>"
"<script>"
"const inputs = document.querySelectorAll('.pin-input input');"
"inputs.forEach((input, i) => {"
"  input.addEventListener('input', (e) => {"
"    if (e.target.value.length === 1 && i < inputs.length - 1) {"
"      inputs[i + 1].focus();"
"    }"
"  });"
"  input.addEventListener('keydown', (e) => {"
"    if (e.key === 'Backspace' && !e.target.value && i > 0) {"
"      inputs[i - 1].focus();"
"    }"
"  });"
"});"
"document.getElementById('pinForm').addEventListener('submit', (e) => {"
"  let pin = '';"
"  inputs.forEach(i => pin += i.value);"
"  document.getElementById('pinValue').value = pin;"
"});"
"if (window.location.search.includes('error=1')) {"
"  document.getElementById('error').classList.add('show');"
"}"
"</script>"
"</body>"
"</html>";

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
".check { color: #28a745; font-size: 14px; margin-bottom: 20px; text-align: center; }"
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
"<div class=\"check\">&#10003; PIN Verified Successfully</div>"
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

static const char *HTML_STEP1_PAGE_TEMPLATE =
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>Step 1: WiFi Setup</title>"
"<style>"
"* { box-sizing: border-box; margin: 0; padding: 0; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
"       background: linear-gradient(135deg, #1e3a5f 0%, #2d5a87 100%); "
"       min-height: 100vh; display: flex; align-items: center; justify-content: center; "
"       padding: 20px; }"
".container { background: white; border-radius: 16px; padding: 28px; "
"             max-width: 400px; width: 100%; box-shadow: 0 20px 60px rgba(0,0,0,0.3); }"
"h1 { color: #1e3a5f; font-size: 20px; margin-bottom: 6px; text-align: center; }"
".subtitle { color: #666; font-size: 13px; margin-bottom: 16px; text-align: center; }"
".progress { display: flex; margin-bottom: 20px; }"
".progress-step { flex: 1; height: 4px; background: #e0e0e0; margin-right: 8px; border-radius: 2px; }"
".progress-step.active { background: #1e3a5f; }"
".progress-step:last-child { margin-right: 0; }"
"label { display: block; color: #333; font-weight: 500; margin-bottom: 8px; font-size: 14px; }"
"select, input[type=password] { width: 100%; padding: 14px; border: 2px solid #e0e0e0; "
"       border-radius: 8px; font-size: 15px; margin-bottom: 12px; background: white; }"
"select:focus, input:focus { outline: none; border-color: #1e3a5f; }"
".btn { display: block; width: 100%; padding: 14px; background: #1e3a5f; color: white; "
"       border: none; border-radius: 8px; font-size: 15px; font-weight: 600; cursor: pointer; }"
".btn:hover { background: #2d5a87; }"
".btn:disabled { background: #ccc; cursor: not-allowed; }"
".btn-scan { background: #28a745; margin-bottom: 12px; }"
".network-list { max-height: 200px; overflow-y: auto; border: 2px solid #e0e0e0; "
"                border-radius: 8px; margin-bottom: 12px; }"
".network-item { padding: 12px 16px; border-bottom: 1px solid #eee; cursor: pointer; "
"                display: flex; justify-content: space-between; align-items: center; }"
".network-item:last-child { border-bottom: none; }"
".network-item:hover { background: #f5f5f5; }"
".network-item.selected { background: #e3f2fd; border-color: #2196f3; }"
".network-name { font-weight: 500; color: #333; }"
".network-signal { font-size: 12px; color: #666; }"
".loading { text-align: center; padding: 40px; color: #666; }"
".spinner { display: inline-block; width: 24px; height: 24px; border: 3px solid #e0e0e0; "
"           border-top-color: #1e3a5f; border-radius: 50%; animation: spin 1s linear infinite; }"
"@keyframes spin { to { transform: rotate(360deg); } }"
".error { background: #ffebee; color: #c62828; padding: 12px; border-radius: 8px; "
"         margin-bottom: 12px; font-size: 13px; }"
".hidden { display: none; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"<div class=\"progress\">"
"<div class=\"progress-step active\"></div>"
"<div class=\"progress-step\"></div>"
"<div class=\"progress-step\"></div>"
"</div>"
"<h1>&#128246; Step 1: WiFi Setup</h1>"
"<p class=\"subtitle\">Connect your thermostat to your home network</p>"
"<div id=\"error\" class=\"error hidden\"></div>"
"<div id=\"loading\" class=\"loading\">"
"<div class=\"spinner\"></div>"
"<p style=\"margin-top:12px;\">Scanning for networks...</p>"
"</div>"
"<div id=\"networkSection\" class=\"hidden\">"
"<label>Select your WiFi network:</label>"
"<div id=\"networkList\" class=\"network-list\"></div>"
"<button class=\"btn btn-scan\" onclick=\"scanNetworks()\">&#128260; Refresh List</button>"
"<div id=\"passwordSection\" class=\"hidden\">"
"<label for=\"password\">WiFi Password:</label>"
"<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter WiFi password\">"
"</div>"
"<form action=\"/setup/step1\" method=\"POST\" id=\"wifiForm\">"
"<input type=\"hidden\" id=\"ssid\" name=\"ssid\" value=\"\">"
"<input type=\"hidden\" id=\"wifi_password\" name=\"password\" value=\"\">"
"<button type=\"submit\" class=\"btn\" id=\"connectBtn\" disabled>Connect &rarr;</button>"
"</form>"
"</div>"
"</div>"
"<script>"
"var selectedNetwork = null;"
"function scanNetworks() {"
"  document.getElementById('loading').classList.remove('hidden');"
"  document.getElementById('networkSection').classList.add('hidden');"
"  document.getElementById('error').classList.add('hidden');"
"  fetch('/api/scan-wifi')"
"    .then(function(r) { return r.json(); })"
"    .then(function(data) {"
"      document.getElementById('loading').classList.add('hidden');"
"      document.getElementById('networkSection').classList.remove('hidden');"
"      var list = document.getElementById('networkList');"
"      list.innerHTML = '';"
"      if (data.networks && data.networks.length > 0) {"
"        data.networks.forEach(function(net) {"
"          var div = document.createElement('div');"
"          div.className = 'network-item';"
"          div.innerHTML = '<span class=\"network-name\">' + net.ssid + '</span>' +"
"            '<span class=\"network-signal\">' + net.rssi + ' dBm</span>';"
"          div.onclick = function() { selectNetwork(net.ssid, div); };"
"          list.appendChild(div);"
"        });"
"      } else {"
"        list.innerHTML = '<div style=\"padding:20px;text-align:center;color:#666;\">No networks found</div>';"
"      }"
"    })"
"    .catch(function(err) {"
"      document.getElementById('loading').classList.add('hidden');"
"      document.getElementById('error').textContent = 'Scan failed: ' + err.message;"
"      document.getElementById('error').classList.remove('hidden');"
"      document.getElementById('networkSection').classList.remove('hidden');"
"    });"
"}"
"function selectNetwork(ssid, el) {"
"  selectedNetwork = ssid;"
"  document.querySelectorAll('.network-item').forEach(function(item) {"
"    item.classList.remove('selected');"
"  });"
"  el.classList.add('selected');"
"  document.getElementById('ssid').value = ssid;"
"  document.getElementById('passwordSection').classList.remove('hidden');"
"  document.getElementById('password').focus();"
"  document.getElementById('connectBtn').disabled = false;"
"}"
"document.getElementById('password').addEventListener('input', function(e) {"
"  document.getElementById('wifi_password').value = e.target.value;"
"});"
"scanNetworks();"
"</script>"
"</body>"
"</html>";

// -----------------------------------------------------------------------------
// DNS Server (Captive Portal)
// -----------------------------------------------------------------------------

// Simple DNS response that redirects all queries to our IP (192.168.4.1)
static void dns_server_task(void *pvParameters)
{
    log_post(LOG_LEVEL_INFO, TAG, "DNS server starting...");

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    uint8_t rx_buffer[DNS_MAX_LEN];
    uint8_t tx_buffer[DNS_MAX_LEN];

    // Create UDP socket
    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_socket < 0) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to create DNS socket");
        vTaskDelete(NULL);
        return;
    }

    // Bind to port 53
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to bind DNS socket");
        close(s_dns_socket);
        s_dns_socket = -1;
        vTaskDelete(NULL);
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG, "DNS server listening on port 53");

    // Our AP IP address (192.168.4.1)
    uint8_t our_ip[4] = {192, 168, 4, 1};

    while (s_dns_socket >= 0) {
        int len = recvfrom(s_dns_socket, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *)&client_addr, &client_len);
        
        if (len < 12) continue;  // DNS header is 12 bytes minimum

        // Copy the query to response buffer
        memcpy(tx_buffer, rx_buffer, len);

        // Modify header to make it a response
        tx_buffer[2] = 0x81;  // QR=1 (response), Opcode=0, AA=1
        tx_buffer[3] = 0x80;  // RA=1
        tx_buffer[6] = 0x00;  // ANCOUNT = 1
        tx_buffer[7] = 0x01;

        // Add answer section (pointing to 192.168.4.1)
        int answer_offset = len;
        tx_buffer[answer_offset++] = 0xC0;  // Pointer to question name
        tx_buffer[answer_offset++] = 0x0C;
        tx_buffer[answer_offset++] = 0x00;  // Type A
        tx_buffer[answer_offset++] = 0x01;
        tx_buffer[answer_offset++] = 0x00;  // Class IN
        tx_buffer[answer_offset++] = 0x01;
        tx_buffer[answer_offset++] = 0x00;  // TTL = 60 seconds
        tx_buffer[answer_offset++] = 0x00;
        tx_buffer[answer_offset++] = 0x00;
        tx_buffer[answer_offset++] = 0x3C;
        tx_buffer[answer_offset++] = 0x00;  // RDLENGTH = 4
        tx_buffer[answer_offset++] = 0x04;
        tx_buffer[answer_offset++] = our_ip[0];  // IP address
        tx_buffer[answer_offset++] = our_ip[1];
        tx_buffer[answer_offset++] = our_ip[2];
        tx_buffer[answer_offset++] = our_ip[3];

        // Send response
        sendto(s_dns_socket, tx_buffer, answer_offset, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    log_post(LOG_LEVEL_INFO, TAG, "DNS server stopped");
    vTaskDelete(NULL);
}

static void start_dns_server(void)
{
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &s_dns_task_handle);
}

static void stop_dns_server(void)
{
    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    if (s_dns_task_handle) {
        // Task will delete itself when socket closes
        s_dns_task_handle = NULL;
    }
}

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

// Captive portal detection endpoints
static esp_err_t handler_captive_portal(httpd_req_t *req)
{
    log_post(LOG_LEVEL_DEBUG, TAG, "Captive portal redirect: %s", req->uri);
    
    // Redirect to PIN page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handler_root(httpd_req_t *req)
{
    log_post(LOG_LEVEL_DEBUG, TAG, "HTTP GET /");
    
    // If PIN not verified, show PIN page
    if (!s_pin_verified) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, HTML_PIN_PAGE, strlen(HTML_PIN_PAGE));
        return ESP_OK;
    }
    
    // PIN verified - show landing page
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_LANDING_PAGE, strlen(HTML_LANDING_PAGE));
    return ESP_OK;
}

static esp_err_t handler_verify_pin(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "HTTP POST /verify-pin");

    // Read POST body
    char buf[128] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "PIN POST body: %s", buf);

    // Parse PIN from form data (pin=XXXX or d1=X&d2=X&d3=X&d4=X&pin=XXXX)
    char *pin_start = strstr(buf, "pin=");
    if (pin_start) {
        pin_start += 4;
        char entered_pin[5] = {0};
        strncpy(entered_pin, pin_start, 4);
        
        log_post(LOG_LEVEL_INFO, TAG, "Entered PIN: %s, Expected: %s", entered_pin, s_pin);

        if (strcmp(entered_pin, s_pin) == 0) {
            // PIN correct!
            s_pin_verified = true;
            log_post(LOG_LEVEL_INFO, TAG, "PIN verified successfully!");
            
            // Redirect to landing page
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }

    // Wrong PIN - redirect back with error
    log_post(LOG_LEVEL_WARN, TAG, "Invalid PIN entered");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/?error=1");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handler_step1_get(httpd_req_t *req)
{
    // Require PIN verification first
    if (!s_pin_verified) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "HTTP GET /setup/step1");

    // Step 1 is now WiFi selection - no template substitution needed
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_STEP1_PAGE_TEMPLATE, strlen(HTML_STEP1_PAGE_TEMPLATE));
    return ESP_OK;
}

// WiFi scan endpoint - returns JSON list of networks
static esp_err_t handler_scan_wifi(httpd_req_t *req)
{
    if (!s_pin_verified) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Not authorized");
        return ESP_FAIL;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Starting WiFi scan...");

    // Configure and start scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);  // Blocking scan
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Scan failed\",\"networks\":[]}");
        return ESP_OK;
    }

    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count > 20) ap_count = 20;  // Limit to 20 networks
    
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Out of memory\",\"networks\":[]}");
        return ESP_OK;
    }

    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    log_post(LOG_LEVEL_INFO, TAG, "Found %d networks", ap_count);

    // Build JSON response
    char *json_buf = malloc(2048);
    if (!json_buf) {
        free(ap_records);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"Out of memory\",\"networks\":[]}");
        return ESP_OK;
    }

    int offset = snprintf(json_buf, 2048, "{\"networks\":[");
    
    for (int i = 0; i < ap_count; i++) {
        // Skip empty SSIDs and duplicates
        if (strlen((char *)ap_records[i].ssid) == 0) continue;
        
        // Add comma between entries
        if (offset > 15) {  // After first entry
            offset += snprintf(json_buf + offset, 2048 - offset, ",");
        }
        
        offset += snprintf(json_buf + offset, 2048 - offset,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
            ap_records[i].ssid,
            ap_records[i].rssi,
            ap_records[i].authmode);
    }

    snprintf(json_buf + offset, 2048 - offset, "]}");

    free(ap_records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_buf);
    free(json_buf);

    return ESP_OK;
}

static esp_err_t handler_step1_post(httpd_req_t *req)
{
    // Require PIN verification first
    if (!s_pin_verified) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "HTTP POST /setup/step1 - WiFi credentials");

    // Read POST body
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "POST body: %s", buf);

    // Parse SSID and password from form data
    char ssid[64] = {0};
    char password[64] = {0};
    
    // Parse ssid=xxx
    char *ssid_start = strstr(buf, "ssid=");
    if (ssid_start) {
        ssid_start += 5;  // strlen("ssid=")
        char *ssid_end = strchr(ssid_start, '&');
        size_t len = ssid_end ? (size_t)(ssid_end - ssid_start) : strlen(ssid_start);
        if (len >= sizeof(ssid)) len = sizeof(ssid) - 1;
        strncpy(ssid, ssid_start, len);
        // Basic URL decode for + -> space
        for (char *p = ssid; *p; p++) {
            if (*p == '+') *p = ' ';
        }
    }

    // Parse password=xxx
    char *pwd_start = strstr(buf, "password=");
    if (pwd_start) {
        pwd_start += 9;  // strlen("password=")
        char *pwd_end = strchr(pwd_start, '&');
        size_t len = pwd_end ? (size_t)(pwd_end - pwd_start) : strlen(pwd_start);
        if (len >= sizeof(password)) len = sizeof(password) - 1;
        strncpy(password, pwd_start, len);
        // Basic URL decode for + -> space
        for (char *p = password; *p; p++) {
            if (*p == '+') *p = ' ';
        }
    }

    if (strlen(ssid) == 0) {
        log_post(LOG_LEVEL_ERROR, TAG, "No SSID provided");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Received WiFi credentials - SSID: %s", ssid);

    // Store credentials in NVS
    prov_wifi_creds_t creds = {0};
    strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
    strncpy(creds.password, password, sizeof(creds.password) - 1);
    
    if (provisioning_set_wifi_creds(&creds) == APP_ERR_OK) {
        log_post(LOG_LEVEL_INFO, TAG, "WiFi credentials saved to NVS");
    } else {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to save WiFi credentials");
    }

    // Redirect to step 2 (connecting page)
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/setup/step2");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t handler_step2_get(httpd_req_t *req)
{
    // Require PIN verification first
    if (!s_pin_verified) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Read the saved WiFi credentials to display the SSID
    prov_wifi_creds_t creds;
    if (provisioning_get_wifi_creds(&creds) != APP_ERR_OK) {
        // No credentials saved, redirect back to step 1
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/setup/step1");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Step 2: Connecting to '%s'", creds.ssid);

    // Connecting page - shows status and triggers connection
    const char *html_template = 
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
        "<title>Step 2: Connecting</title>"
        "<style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:20px;background:#f5f5f5;}"
        ".card{background:white;border-radius:12px;padding:30px;max-width:400px;margin:20px auto;"
        "box-shadow:0 2px 8px rgba(0,0,0,0.1);text-align:center;}"
        "h1{color:#333;font-size:20px;margin-bottom:10px;}"
        ".ssid{color:#007aff;font-weight:600;word-break:break-all;}"
        ".spinner{width:50px;height:50px;border:4px solid #e0e0e0;border-top:4px solid #007aff;"
        "border-radius:50%%;animation:spin 1s linear infinite;margin:20px auto;}"
        "@keyframes spin{0%%{transform:rotate(0deg)}100%%{transform:rotate(360deg)}}"
        ".status{color:#666;margin:15px 0;}"
        ".instructions{background:#fff8e1;border-radius:8px;padding:15px;margin-top:20px;"
        "text-align:left;font-size:14px;border-left:4px solid #ffc107;}"
        ".instructions h3{margin:0 0 10px 0;color:#f57c00;font-size:14px;}"
        ".instructions ol{margin:0;padding-left:20px;}"
        ".instructions li{margin:5px 0;color:#666;}"
        ".done{color:#4caf50;display:none;}"
        ".error{color:#f44336;display:none;}"
        "</style>"
        "</head><body>"
        "<div class=\"card\">"
        "<h1>Connecting to WiFi</h1>"
        "<p class=\"ssid\">%s</p>"
        "<div class=\"spinner\" id=\"spinner\"></div>"
        "<p class=\"status\" id=\"status\">Preparing to connect...</p>"
        "<p class=\"done\" id=\"done\">✓ Connection initiated!</p>"
        "<p class=\"error\" id=\"error\"></p>"
        "<div class=\"instructions\">"
        "<h3>📱 What happens next:</h3>"
        "<ol>"
        "<li>The thermostat will disconnect from this setup network</li>"
        "<li>It will connect to your home WiFi</li>"
        "<li>On your phone, go back to your WiFi settings</li>"
        "<li>Connect to your home WiFi: <strong>%s</strong></li>"
        "<li>The thermostat LCD will show its new IP address</li>"
        "<li>Open that IP in your browser to complete setup</li>"
        "</ol>"
        "</div>"
        "</div>"
        "<script>"
        "setTimeout(function(){"
        "  document.getElementById('status').textContent='Triggering connection...';"
        "  fetch('/api/connect-wifi',{method:'POST'})"
        "    .then(function(r){return r.json();})"
        "    .then(function(d){"
        "      document.getElementById('spinner').style.display='none';"
        "      if(d.success){"
        "        document.getElementById('status').style.display='none';"
        "        document.getElementById('done').style.display='block';"
        "      }else{"
        "        document.getElementById('error').textContent='Error: '+d.error;"
        "        document.getElementById('error').style.display='block';"
        "      }"
        "    })"
        "    .catch(function(e){"
        "      document.getElementById('status').textContent='Connection in progress...';"
        "      document.getElementById('spinner').style.display='none';"
        "      document.getElementById('done').style.display='block';"
        "    });"
        "},1500);"
        "</script>"
        "</body></html>";

    // Format the HTML with the SSID (shown twice in the template)
    char *html_buf = malloc(4096);
    if (!html_buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    snprintf(html_buf, 4096, html_template, creds.ssid, creds.ssid);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_buf, strlen(html_buf));
    free(html_buf);

    return ESP_OK;
}

// Handler to trigger WiFi connection - called via JavaScript fetch
static esp_err_t handler_connect_wifi(httpd_req_t *req)
{
    // Require PIN verification
    if (!s_pin_verified) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Not authorized\"}");
        return ESP_OK;
    }

    // Verify we have saved credentials
    prov_wifi_creds_t creds;
    if (provisioning_get_wifi_creds(&creds) != APP_ERR_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No WiFi credentials\"}");
        return ESP_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Connect requested for '%s'", creds.ssid);

    // Mark that connection is pending - a background task will handle the actual switch
    s_connect_pending = true;

    // Send success response immediately
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true,\"message\":\"Connection initiated\"}");

    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Step 3: API Key Setup (served on STA mode after WiFi connected)
// -----------------------------------------------------------------------------

// Template with %s placeholder for the device IP address
// Compact Step 3 page - API Key setup (must fit in buffer)
static const char *HTML_STEP3_PAGE_TEMPLATE =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>API Key</title><style>"
"*{box-sizing:border-box}body{font-family:sans-serif;margin:0;padding:20px;"
"background:linear-gradient(135deg,#667eea,#764ba2);min-height:100vh}"
".c{max-width:400px;margin:0 auto;background:#fff;padding:20px;border-radius:12px}"
"h1{text-align:center;margin:0 0 15px}input{width:100%%;padding:12px;font-size:16px;"
"border:2px solid #ddd;border-radius:6px;margin:10px 0}.btn{width:100%%;padding:12px;"
"font-size:16px;color:#fff;background:#667eea;border:none;border-radius:6px;cursor:pointer}"
".ok{background:#d4edda;color:#155724;padding:10px;border-radius:6px;display:none;text-align:center}"
".err{background:#f8d7da;color:#721c24;padding:10px;border-radius:6px;display:none;text-align:center}"
".spin{display:none;text-align:center;padding:30px}"
".ld{width:40px;height:40px;border:4px solid #eee;border-top:4px solid #667eea;"
"border-radius:50%%;animation:sp 1s linear infinite;margin:0 auto}"
"@keyframes sp{to{transform:rotate(360deg)}}"
"#d{margin-top:15px;padding:8px;background:#ffc;font-size:10px;word-break:break-all}"
"</style></head><body><div class=\"c\">"
"<div class=\"spin\" id=\"sp\"><div class=\"ld\"></div><p>Sending...</p></div>"
"<div id=\"m\"><h1>API Key Setup</h1><p>IP: <b>%s</b></p>"
"<p>Scan QR code from dashboard or paste key:</p>"
"<input type=\"text\" id=\"k\" placeholder=\"Paste API key\">"
"<button class=\"btn\" onclick=\"go()\">Submit</button></div>"
"<div class=\"ok\" id=\"ok\">Success! Restarting...</div>"
"<div class=\"err\" id=\"er\"></div>"
"<div id=\"d\"></div></div>"
"<script>"
"var d=document.getElementById('d');"
"function L(s){d.innerHTML+=s+'<br>';}"
"L('URL:'+location.search);"
"function go(key){"
"if(!key)key=document.getElementById('k').value.trim();"
"if(key.length<10){document.getElementById('er').innerHTML='Key too short';document.getElementById('er').style.display='block';return;}"
"document.getElementById('sp').style.display='block';document.getElementById('m').style.display='none';"
"L('POST...');"
"fetch('/setup/step3',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
"body:'api_key='+encodeURIComponent(key)}).then(function(r){L('S:'+r.status);return r.json();})"
".then(function(j){L('R:'+JSON.stringify(j));if(j.success){document.getElementById('sp').style.display='none';"
"document.getElementById('ok').style.display='block';}else{document.getElementById('sp').style.display='none';"
"document.getElementById('er').innerHTML=j.error||'Failed';document.getElementById('er').style.display='block';"
"document.getElementById('m').style.display='block';}})"
".catch(function(e){L('E:'+e);document.getElementById('sp').style.display='none';"
"document.getElementById('er').innerHTML='Network error';document.getElementById('er').style.display='block';"
"document.getElementById('m').style.display='block';});}"
"var u=new URLSearchParams(location.search);var k=u.get('api_key');"
"L('Key:'+(k?k.substring(0,8)+'...':'none'));"
"if(k&&k.length>10){L('Auto submit');document.getElementById('k').value=k;setTimeout(function(){go(k);},300);}"
"</script></body></html>";

// Store the device IP for use in templates
static char s_device_ip[16] = "0.0.0.0";

void setup_server_set_device_ip(const char *ip)
{
    if (ip) {
        strncpy(s_device_ip, ip, sizeof(s_device_ip) - 1);
        s_device_ip[sizeof(s_device_ip) - 1] = '\0';
    }
}

static esp_err_t handler_step3_get(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "HTTP GET /setup/step3 - API Key page");
    
    // Build the page with the device IP
    char *html_buf = malloc(16384);  // 16KB buffer for larger HTML
    if (!html_buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    
    snprintf(html_buf, 16384, HTML_STEP3_PAGE_TEMPLATE, s_device_ip);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_buf, strlen(html_buf));
    free(html_buf);
    return ESP_OK;
}

static esp_err_t handler_step3_post(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "HTTP POST /setup/step3 - API Key submission");

    // Read POST body
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"No data\"}");
        return ESP_OK;
    }

    log_post(LOG_LEVEL_DEBUG, TAG, "POST body: %s", buf);

    // Parse api_key=xxx
    char api_key[128] = {0};
    char *key_start = strstr(buf, "api_key=");
    if (key_start) {
        key_start += 8;  // strlen("api_key=")
        char *key_end = strchr(key_start, '&');
        size_t len = key_end ? (size_t)(key_end - key_start) : strlen(key_start);
        if (len > sizeof(api_key) - 1) len = sizeof(api_key) - 1;
        strncpy(api_key, key_start, len);
    }

    if (strlen(api_key) < 10) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"API key too short\"}");
        return ESP_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Received API key: %s...", api_key);

    // Store API key using provisioning
    if (provisioning_set_api_key(api_key) != APP_ERR_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"Failed to save\"}");
        return ESP_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "API key saved successfully!");
    s_api_key_complete = true;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\":true}");
    return ESP_OK;
}

// Root handler for Step 3 server (redirects to step3)
static esp_err_t handler_step3_root(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/setup/step3");
    httpd_resp_send(req, NULL, 0);
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

    // Create both AP and STA network interfaces
    // STA is needed for WiFi scanning, AP is for the captive portal
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

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

    // Use APSTA mode: AP for captive portal, STA interface enables scanning
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
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
    config.max_uri_handlers = 16;  // Need more handlers for captive portal
    config.max_open_sockets = 4;   // Keep low to avoid exhausting sockets (DNS uses 1)
    config.backlog_conn = 3;       // Small backlog
    config.recv_wait_timeout = 5;  // 5 second receive timeout
    config.send_wait_timeout = 5;  // 5 second send timeout
    config.close_fn = NULL;        // Use default close behavior

    log_post(LOG_LEVEL_INFO, TAG, "Starting HTTP server on port %d", config.server_port);

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    // Register URI handlers

    // Root handler (PIN page or landing page)
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_root,
    };
    httpd_register_uri_handler(s_http_server, &uri_root);

    // PIN verification
    httpd_uri_t uri_verify_pin = {
        .uri = "/verify-pin",
        .method = HTTP_POST,
        .handler = handler_verify_pin,
    };
    httpd_register_uri_handler(s_http_server, &uri_verify_pin);

    // Setup wizard steps
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

    // API endpoints for WiFi operations
    httpd_uri_t uri_scan_wifi = {
        .uri = "/api/scan-wifi",
        .method = HTTP_GET,
        .handler = handler_scan_wifi,
    };
    httpd_register_uri_handler(s_http_server, &uri_scan_wifi);

    httpd_uri_t uri_connect_wifi = {
        .uri = "/api/connect-wifi",
        .method = HTTP_POST,
        .handler = handler_connect_wifi,
    };
    httpd_register_uri_handler(s_http_server, &uri_connect_wifi);

    // Captive portal detection endpoints (various OS/browser checks)
    const char *captive_uris[] = {
        "/generate_204",           // Android
        "/gen_204",                // Android
        "/hotspot-detect.html",    // iOS/macOS
        "/library/test/success.html", // iOS
        "/ncsi.txt",               // Windows
        "/connecttest.txt",        // Windows
        "/redirect",               // Windows 11
        "/success.txt",            // Various
        "/canonical.html",         // Firefox
    };

    for (int i = 0; i < sizeof(captive_uris) / sizeof(captive_uris[0]); i++) {
        httpd_uri_t uri = {
            .uri = captive_uris[i],
            .method = HTTP_GET,
            .handler = handler_captive_portal,
        };
        httpd_register_uri_handler(s_http_server, &uri);
    }

    log_post(LOG_LEVEL_INFO, TAG, "HTTP server started with captive portal support");
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

    // Generate security PIN
    generate_pin();

    // Reset PIN verification state
    s_pin_verified = false;

    // Initialize SoftAP
    app_error_t err = init_softap();
    if (err != APP_ERR_OK) {
        return err;
    }

    // Start DNS server for captive portal
    start_dns_server();

    // Initialize HTTP server
    err = init_http_server();
    if (err != APP_ERR_OK) {
        stop_dns_server();
        return err;
    }

    s_server_running = true;
    log_post(LOG_LEVEL_INFO, TAG, "Setup server ready! PIN: %s", s_pin);

    return APP_ERR_OK;
}

app_error_t setup_server_stop(void)
{
    if (!s_server_running) {
        return APP_ERR_OK;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Stopping setup server...");

    // Stop DNS server
    stop_dns_server();

    // Stop HTTP server
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    // Stop WiFi
    esp_wifi_stop();
    esp_wifi_deinit();

    s_server_running = false;
    s_pin_verified = false;
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

const char* setup_server_get_pin(void)
{
    return s_pin;
}

bool setup_server_connect_pending(void)
{
    return s_connect_pending;
}

// Event handler for STA mode connection
static void sta_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            log_post(LOG_LEVEL_INFO, TAG, "STA started, connecting...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            log_post(LOG_LEVEL_WARN, TAG, "STA disconnected, retrying...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        log_post(LOG_LEVEL_INFO, TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

app_error_t setup_server_connect_wifi(char *ip_addr, size_t buf_len)
{
    if (!s_connect_pending) {
        return ERR_GENERIC;
    }

    // Get saved credentials
    prov_wifi_creds_t creds;
    if (provisioning_get_wifi_creds(&creds) != APP_ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "No WiFi credentials saved");
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG, "Switching to STA mode for '%s'", creds.ssid);

    // Stop the setup server (HTTP, DNS)
    stop_dns_server();
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }
    s_server_running = false;

    // Short delay to ensure response was sent
    vTaskDelay(pdMS_TO_TICKS(500));

    // Stop current APSTA WiFi
    esp_wifi_stop();

    // STA netif already created during init_softap (for scanning)
    // Just re-register event handlers for STA mode
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, sta_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, sta_event_handler, NULL);

    // Configure for STA mode (no longer need AP)
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, creds.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, creds.password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection (timeout 15 seconds)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            pdMS_TO_TICKS(15000));

    if (!(bits & WIFI_CONNECTED_BIT)) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to connect to WiFi");
        s_connect_pending = false;
        return ERR_GENERIC;
    }

    // Get assigned IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        if (ip_addr && buf_len >= 16) {
            snprintf(ip_addr, buf_len, IPSTR, IP2STR(&ip_info.ip));
        }
        log_post(LOG_LEVEL_INFO, TAG, "Connected! IP: " IPSTR, IP2STR(&ip_info.ip));
    }

    s_connect_pending = false;
    s_setup_complete = true;  // Mark that WiFi setup is done

    return APP_ERR_OK;
}

app_error_t setup_server_start_step3(void)
{
    log_post(LOG_LEVEL_INFO, TAG, "Starting Step 3 HTTP server on STA...");

    // Make sure previous server is stopped
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 8;
    config.max_open_sockets = 4;

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to start Step 3 server: %s", esp_err_to_name(err));
        return ERR_GENERIC;
    }

    // Register Step 3 handlers
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_step3_root,
    };
    httpd_register_uri_handler(s_http_server, &uri_root);

    httpd_uri_t uri_step3_get = {
        .uri = "/setup/step3",
        .method = HTTP_GET,
        .handler = handler_step3_get,
    };
    httpd_register_uri_handler(s_http_server, &uri_step3_get);

    httpd_uri_t uri_step3_post = {
        .uri = "/setup/step3",
        .method = HTTP_POST,
        .handler = handler_step3_post,
    };
    httpd_register_uri_handler(s_http_server, &uri_step3_post);

    s_server_running = true;
    log_post(LOG_LEVEL_INFO, TAG, "Step 3 server ready - browse to device IP");

    return APP_ERR_OK;
}

bool setup_server_api_key_complete(void)
{
    return s_api_key_complete;
}
