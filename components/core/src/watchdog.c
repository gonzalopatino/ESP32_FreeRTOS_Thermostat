/**
 * @file    watchdog.c
 * @brief   Task watchdog monitoring system.
 *
 * Implements a software watchdog system to monitor task health.
 * Each registered task must periodically feed the watchdog to
 * indicate healthy operation. Timeout triggers system recovery.
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

#include "core/watchdog.h"
#include "core/logging.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

esp_err_t watchdog_init(void)
{
    log_post(LOG_LEVEL_INFO, "WDT", "Watchdog init stub");
    return ESP_OK;
}

esp_err_t watchdog_register_current(const char *task_name) {
    log_post(LOG_LEVEL_INFO, "WDT", "Register task: %s", task_name);
    return ESP_OK;
}


esp_err_t watchdog_feed(void) {
    // In a real implementation, call esp_task_wdt_reset() here.
    return ESP_OK;
}