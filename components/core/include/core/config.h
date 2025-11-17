#ifndef CONFIG_H
#define CONFIG_H

#define APP_NAME        "ThermostatRTOS"
#define APP_FW_VERSION  "0.1.0"

// Task priorities
#define TASK_PRIO_LOGGER      5
#define TASK_PRIO_SENSORS     4
#define TASK_PRIO_HEARTBEAT   3

// Stack sizes
#define TASK_STACK_LOGGER     4096
#define TASK_STACK_SENSORS    4096
#define TASK_STACK_HEARTBEAT  4096

// Periods
#define PERIOD_LOGGER_MS      50
#define PERIOD_SENSORS_MS     500

// Logging
#define LOG_BUFFER_LEN        256
#define LOG_QUEUE_LENGTH      32

//Control
#define TASK_PRIO_CONTROL       5
#define TASK_STACK_CONTROL      4096


// Thermostat control parameters
#define THERMOSTAT_SETPOINT_C       22.0f
#define THERMOSTAT_HYSTERESIS_C      0.5f

// Board pins
#define LED_GPIO              2   // For heartbeat testing
#define GPIO_HEAT_OUTPUT      4

#endif
