/**
 * @file    task_common.c
 * @brief   Shared inter-task communication queues.
 *
 * Initializes and provides access to queues used for
 * communication between FreeRTOS tasks.
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

#include "app/task_common.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * GLOBAL VARIABLES
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Queue for sensor samples (sensors -> control) */
QueueHandle_t g_q_sensor_samples    = NULL;

/** Queue for thermostat state (control -> display) */
QueueHandle_t g_q_thermostat_state  = NULL;

/** Queue for telemetry state (control -> network) */
QueueHandle_t g_q_telemetry_state   = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

void tasks_common_init_queues(void)
{
    g_q_sensor_samples   = xQueueCreate(1, sizeof(sensor_sample_t));
    g_q_thermostat_state = xQueueCreate(1, sizeof(thermostat_state_t));
    g_q_telemetry_state  = xQueueCreate(1, sizeof(thermostat_state_t));

    if (!g_q_sensor_samples || !g_q_thermostat_state || !g_q_telemetry_state) {
        error_report(ERR_GENERIC, "tasks_common_init_queues: queue alloc failed");
    }
}
