#ifndef TASK_COMMON_H
#define TASK_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/app_types.h"

// Queue from Sensor task to Control task
extern QueueHandle_t g_q_sensor_samples;

// Create queues used by application tasks
void tasks_common_init_queues(void);

#endif
