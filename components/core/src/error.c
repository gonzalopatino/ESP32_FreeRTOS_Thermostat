/**
 * @file    error.c
 * @brief   Error handling and reporting utilities.
 *
 * Provides centralized error handling for both recoverable and
 * fatal error conditions. Supports logging integration and
 * configurable recovery strategies.
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

#include "core/error.h"
#include "core/logging.h"

#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Report a non-fatal error condition.
 */
void error_report(app_error_t err, const char *context)
{
    log_post(LOG_LEVEL_ERROR, "ERROR", "err=%d context=%s", (int)err, context);
}

void error_fatal(app_error_t err, const char *context) {
    log_post(LOG_LEVEL_ERROR, "FATAL", "err=%d context=%s", (int)err, context);

    // For now abort. Later you can call esp_restart() instead.
    abort();
}