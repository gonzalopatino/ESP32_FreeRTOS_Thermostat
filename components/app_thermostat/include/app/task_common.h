#ifndef TASK_COMMON_H
#define TASK_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "core/app_types.h"     // sensor_sample_t
#include "core/thermostat.h"    // thermostat_state_t
#include "core/error.h"

// Queue of latest sensor samples (producer: SENSORS, consumer: CONTROL)
extern QueueHandle_t g_q_sensor_samples;

// Queue of latest thermostat state (producer: CONTROL, consumers: DISPLAY, maybe TELEMETRY)
extern QueueHandle_t g_q_thermostat_state;

// Initialize all shared queues used by tasks
void tasks_common_init_queues(void);

#endif  // TASK_COMMON_H
