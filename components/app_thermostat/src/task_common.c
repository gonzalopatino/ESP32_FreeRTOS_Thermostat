#include "app/task_common.h"

// Global queue used to pass temperature samples
// from the sensor task → to the control task.
// Declared here so multiple tasks can reference it.
QueueHandle_t g_q_sensor_samples = NULL;


/**
 * @brief Create and initialize all shared inter-task queues.
 *
 * This function is called once during system startup (from app_main).
 * By keeping queue creation centralized here instead of inside each task,
 * we ensure:
 *   - consistent queue configuration
 *   - clear ownership and lifecycle management
 *   - easier maintenance as the system grows
 *
 * IMPORTANT:
 *   This queue intentionally uses a small length (8 entries) because the
 *   control task only needs the **most recent sensor sample**, not a backlog.
 *   A larger queue would just add latency without adding value.
 */
void tasks_common_init_queues(void) {

    // Create a queue that holds up to 8 temperature samples.
    // If the queue fills up, newer writes will overwrite (depending on
    // task behavior), which is acceptable because old sensor data is
    // not useful for control decisions in a thermostat.
    g_q_sensor_samples = xQueueCreate(8, sizeof(sensor_sample_t));
}
