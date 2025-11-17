#ifndef TASK_CONTROL_H
#define TASK_CONTROL_H

/**
 * @brief Start the thermostat control task.
 *
 * This task:
 *   - waits on g_q_sensor_samples for new sensor_sample_t frames
 *   - applies hysteresis control around THERMOSTAT_SETPOINT_C
 *   - drives GPIO_HEAT_OUTPUT accordingly
 *   - logs decisions via JSON logger
 */
void task_control_start(void);

#endif
