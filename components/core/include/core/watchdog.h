#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "esp_err.h"

// Initialize task watchdog (stub for now)
esp_err_t watchdog_init(void);

// Register the current task with the watchdog (stub)
esp_err_t watchdog_register_current(const char *task_name);

//Feed (reset) the watchdog (stub)
esp_err_t watchdog_feed(void);

#endif