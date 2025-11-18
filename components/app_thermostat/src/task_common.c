#include "app/task_common.h"

// Global queues shared between tasks
QueueHandle_t g_q_sensor_samples    = NULL;
QueueHandle_t g_q_thermostat_state  = NULL;

void tasks_common_init_queues(void) {
    // Overwrite queue since control only needs latest sample
    g_q_sensor_samples = xQueueCreate(
        1,                         // queue length
        sizeof(sensor_sample_t)    // element size
    );

    if (g_q_sensor_samples == NULL) {
    error_fatal(ERR_QUEUE_CREATE_FAILED, "g_q_sensor_samples");
    }


    // Overwrite queue for thermostat state as well
    g_q_thermostat_state = xQueueCreate(
        1,                         // depth, tune as needed
        sizeof(thermostat_state_t) //element size
    );

    if (g_q_thermostat_state == NULL){
        error_fatal(ERR_QUEUE_CREATE_FAILED, "g_q_thermostat_state");
    }
}
