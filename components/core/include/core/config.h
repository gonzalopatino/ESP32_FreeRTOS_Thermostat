#ifndef CONFIG_H
#define CONFIG_H

#define APP_NAME        "ThermostatRTOS"
#define APP_FW_VERSION  "0.1.0"

// Task priorities
#define TASK_PRIO_HEARTBEAT   3

// Stack sizes
#define TASK_STACK_HEARTBEAT  4096

// Board pins
#define LED_GPIO              2   // For heartbeat testing

#endif
