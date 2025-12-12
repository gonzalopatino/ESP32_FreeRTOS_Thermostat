/**
 * @file    api_server.c
 * @brief   Runtime HTTP API server for remote thermostat control.
 *
 * Implements HTTP endpoints for reading and updating thermostat configuration
 * from the cloud platform or local network clients.
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * INCLUDES
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "app/api_server.h"
#include "core/logging.h"
#include "core/thermostat_config.h"
#include "core/thermostat.h"

#include "esp_http_server.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char *TAG = "API_SRV";

/** Maximum content length for POST body */
#define MAX_POST_BODY_LEN  512

/** API server port */
#define API_SERVER_PORT    80

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE VARIABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

static httpd_handle_t s_server = NULL;
static bool s_running = false;

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE FUNCTIONS - HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Send JSON error response.
 */
static esp_err_t send_error_response(httpd_req_t *req, int status, const char *message)
{
    httpd_resp_set_status(req, status == 401 ? "401 Unauthorized" : 
                                status == 400 ? "400 Bad Request" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    
    char body[128];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    
    return httpd_resp_send(req, body, strlen(body));
}

/**
 * @brief Convert mode enum to string.
 */
static const char* mode_to_string(thermostat_mode_t mode)
{
    switch (mode) {
        case THERMOSTAT_MODE_OFF:  return "OFF";
        case THERMOSTAT_MODE_HEAT: return "HEAT";
        case THERMOSTAT_MODE_COOL: return "COOL";
        case THERMOSTAT_MODE_AUTO: return "AUTO";
        default:                   return "UNKNOWN";
    }
}

/**
 * @brief Convert string to mode enum.
 * @return -1 if invalid mode string
 */
static int string_to_mode(const char *str)
{
    if (strcasecmp(str, "OFF") == 0)  return THERMOSTAT_MODE_OFF;
    if (strcasecmp(str, "HEAT") == 0) return THERMOSTAT_MODE_HEAT;
    if (strcasecmp(str, "COOL") == 0) return THERMOSTAT_MODE_COOL;
    if (strcasecmp(str, "AUTO") == 0) return THERMOSTAT_MODE_AUTO;
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HTTP HANDLERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief GET /api/config - Return current thermostat configuration.
 *
 * Response:
 * {
 *   "setpoint_c": 22.0,
 *   "hysteresis_c": 0.5,
 *   "mode": "HEAT"
 * }
 */
static esp_err_t handler_get_config(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "GET /api/config");

    // No authentication required - local network access only

    // Get current config
    thermostat_config_t cfg;
    if (thermostat_config_get(&cfg) != APP_ERR_OK) {
        return send_error_response(req, 500, "Failed to get config");
    }

    // Get current mode
    thermostat_mode_t mode;
    if (thermostat_get_mode(&mode) != APP_ERR_OK) {
        mode = THERMOSTAT_MODE_OFF;  // Default fallback
    }

    // Build JSON response
    char json_body[192];
    int len = snprintf(json_body, sizeof(json_body),
        "{\"setpoint_c\":%.2f,\"hysteresis_c\":%.2f,\"mode\":\"%s\"}",
        cfg.setpoint_c, cfg.hysteresis_c, mode_to_string(mode));

    if (len <= 0 || len >= (int)sizeof(json_body)) {
        return send_error_response(req, 500, "JSON build error");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    log_post(LOG_LEVEL_INFO, TAG, "Response: %s", json_body);
    return httpd_resp_send(req, json_body, len);
}

/**
 * @brief POST /api/config - Update thermostat configuration.
 *
 * Request body:
 * {
 *   "setpoint_c": 23.0,      // optional
 *   "hysteresis_c": 0.5,     // optional
 *   "mode": "HEAT"           // optional: OFF, HEAT, COOL, AUTO
 * }
 *
 * Response:
 * {
 *   "status": "ok",
 *   "setpoint_c": 23.0,
 *   "hysteresis_c": 0.5,
 *   "mode": "HEAT"
 * }
 */
static esp_err_t handler_post_config(httpd_req_t *req)
{
    log_post(LOG_LEVEL_INFO, TAG, "POST /api/config");

    // No authentication required - local network access only

    // Read request body
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > MAX_POST_BODY_LEN) {
        return send_error_response(req, 400, "Invalid content length");
    }

    char *body = malloc(content_len + 1);
    if (!body) {
        return send_error_response(req, 500, "Memory allocation failed");
    }

    int received = httpd_req_recv(req, body, content_len);
    if (received != content_len) {
        free(body);
        return send_error_response(req, 400, "Failed to read body");
    }
    body[content_len] = '\0';

    log_post(LOG_LEVEL_DEBUG, TAG, "Body: %s", body);

    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);

    if (!json) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    // Get current config
    thermostat_config_t cfg;
    if (thermostat_config_get(&cfg) != APP_ERR_OK) {
        cJSON_Delete(json);
        return send_error_response(req, 500, "Failed to get current config");
    }

    // Update fields if present
    bool updated = false;

    cJSON *setpoint = cJSON_GetObjectItem(json, "setpoint_c");
    if (setpoint && cJSON_IsNumber(setpoint)) {
        float new_sp = (float)setpoint->valuedouble;
        
        // Validate range
        if (new_sp >= THERMOSTAT_SP_MIN_C && new_sp <= THERMOSTAT_SP_MAX_C) {
            cfg.setpoint_c = new_sp;
            updated = true;
            log_post(LOG_LEVEL_INFO, TAG, "Setpoint updated to %.2f°C", new_sp);
        } else {
            cJSON_Delete(json);
            char err_msg[64];
            snprintf(err_msg, sizeof(err_msg), 
                     "Setpoint out of range (%.1f-%.1f)", 
                     THERMOSTAT_SP_MIN_C, THERMOSTAT_SP_MAX_C);
            return send_error_response(req, 400, err_msg);
        }
    }

    cJSON *hysteresis = cJSON_GetObjectItem(json, "hysteresis_c");
    if (hysteresis && cJSON_IsNumber(hysteresis)) {
        float new_hyst = (float)hysteresis->valuedouble;
        
        // Validate range (reasonable hysteresis: 0.1 - 3.0)
        if (new_hyst >= 0.1f && new_hyst <= 3.0f) {
            cfg.hysteresis_c = new_hyst;
            updated = true;
            log_post(LOG_LEVEL_INFO, TAG, "Hysteresis updated to %.2f°C", new_hyst);
        } else {
            cJSON_Delete(json);
            return send_error_response(req, 400, "Hysteresis out of range (0.1-3.0)");
        }
    }

    // Handle mode update
    thermostat_mode_t current_mode;
    thermostat_get_mode(&current_mode);

    cJSON *mode_json = cJSON_GetObjectItem(json, "mode");
    if (mode_json && cJSON_IsString(mode_json)) {
        int new_mode = string_to_mode(mode_json->valuestring);
        if (new_mode >= 0) {
            if (thermostat_set_mode((thermostat_mode_t)new_mode) == APP_ERR_OK) {
                current_mode = (thermostat_mode_t)new_mode;
                log_post(LOG_LEVEL_INFO, TAG, "Mode updated to %s", mode_to_string(current_mode));
            } else {
                cJSON_Delete(json);
                return send_error_response(req, 500, "Failed to set mode");
            }
        } else {
            cJSON_Delete(json);
            return send_error_response(req, 400, "Invalid mode (use OFF, HEAT, COOL, or AUTO)");
        }
    }

    cJSON_Delete(json);

    // Apply new config if changed
    if (updated) {
        if (thermostat_config_set(&cfg) != APP_ERR_OK) {
            return send_error_response(req, 500, "Failed to apply config");
        }
        log_post(LOG_LEVEL_INFO, TAG, 
                 "Config applied: setpoint=%.2f°C hysteresis=%.2f°C",
                 cfg.setpoint_c, cfg.hysteresis_c);
    }

    // Build success response with mode
    char json_resp[192];
    int len = snprintf(json_resp, sizeof(json_resp),
        "{\"status\":\"ok\",\"setpoint_c\":%.2f,\"hysteresis_c\":%.2f,\"mode\":\"%s\"}",
        cfg.setpoint_c, cfg.hysteresis_c, mode_to_string(current_mode));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    return httpd_resp_send(req, json_resp, len);
}

/**
 * @brief OPTIONS /api/config - Handle CORS preflight.
 */
static esp_err_t handler_options_config(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ═══════════════════════════════════════════════════════════════════════════ */

app_error_t api_server_start(void)
{
    if (s_running) {
        log_post(LOG_LEVEL_WARN, TAG, "API server already running");
        return APP_ERR_OK;
    }

    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = API_SERVER_PORT;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 8;

    log_post(LOG_LEVEL_INFO, TAG, "Starting API server on port %d", API_SERVER_PORT);

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to start HTTP server: %s", 
                 esp_err_to_name(err));
        return ERR_GENERIC;
    }

    // Register URI handlers
    httpd_uri_t uri_get_config = {
        .uri      = "/api/config",
        .method   = HTTP_GET,
        .handler  = handler_get_config,
        .user_ctx = NULL
    };

    httpd_uri_t uri_post_config = {
        .uri      = "/api/config",
        .method   = HTTP_POST,
        .handler  = handler_post_config,
        .user_ctx = NULL
    };

    httpd_uri_t uri_options_config = {
        .uri      = "/api/config",
        .method   = HTTP_OPTIONS,
        .handler  = handler_options_config,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(s_server, &uri_get_config);
    httpd_register_uri_handler(s_server, &uri_post_config);
    httpd_register_uri_handler(s_server, &uri_options_config);

    s_running = true;
    log_post(LOG_LEVEL_INFO, TAG, "API server started successfully");
    log_post(LOG_LEVEL_INFO, TAG, "Endpoints: GET/POST /api/config");

    return APP_ERR_OK;
}

app_error_t api_server_stop(void)
{
    if (!s_running || !s_server) {
        return APP_ERR_OK;
    }

    esp_err_t err = httpd_stop(s_server);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to stop HTTP server: %s", 
                 esp_err_to_name(err));
        return ERR_GENERIC;
    }

    s_server = NULL;
    s_running = false;
    log_post(LOG_LEVEL_INFO, TAG, "API server stopped");

    return APP_ERR_OK;
}

bool api_server_is_running(void)
{
    return s_running;
}
