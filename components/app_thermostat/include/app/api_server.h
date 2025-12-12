/**
 * @file    api_server.h
 * @brief   Runtime HTTP API server for remote thermostat control.
 *
 * Provides HTTP endpoints for reading and updating thermostat configuration
 * (setpoint, hysteresis) from the cloud platform or local network.
 *
 * Endpoints:
 *   GET  /api/config  - Returns current setpoint and hysteresis as JSON
 *   POST /api/config  - Updates setpoint and/or hysteresis from JSON body
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef API_SERVER_H
#define API_SERVER_H

#include <stdbool.h>
#include "core/error.h"

/**
 * @brief Start the runtime HTTP API server.
 *
 * Should be called after WiFi is connected and device has an IP address.
 * The server runs on port 80 and provides endpoints for remote configuration.
 *
 * @return APP_ERR_OK on success, error code otherwise.
 */
app_error_t api_server_start(void);

/**
 * @brief Stop the runtime HTTP API server.
 *
 * Stops the HTTP server and releases resources.
 *
 * @return APP_ERR_OK on success, error code otherwise.
 */
app_error_t api_server_stop(void);

/**
 * @brief Check if the API server is currently running.
 *
 * @return true if server is running, false otherwise.
 */
bool api_server_is_running(void);

#endif  /* API_SERVER_H */
