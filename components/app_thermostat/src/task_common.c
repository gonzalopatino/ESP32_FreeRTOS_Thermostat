#include "app/task_common.h"

// Global queues shared between tasks
QueueHandle_t g_q_sensor_samples    = NULL;
QueueHandle_t g_q_thermostat_state  = NULL;
QueueHandle_t g_q_telemetry_state   = NULL;

void tasks_common_init_queues(void)
{
    g_q_sensor_samples   = xQueueCreate(1, sizeof(sensor_sample_t));
    g_q_thermostat_state = xQueueCreate(1, sizeof(thermostat_state_t));
    g_q_telemetry_state  = xQueueCreate(1, sizeof(thermostat_state_t));

    if (!g_q_sensor_samples || !g_q_thermostat_state || !g_q_telemetry_state) {
        // You already have logging/error helpers, keep it simple
        error_report(ERR_GENERIC, "tasks_common_init_queues: queue alloc failed");
    }
}
