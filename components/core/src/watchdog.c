#include "core/watchdog.h"
#include "core/logging.h"

esp_err_t watchdog_init(void){
    log_post(LOG_LEVEL_INFO, "WDT", "Watchdog init stub");
    return ESP_OK;
}

esp_err_t watchdog_register_current(const char *task_name) {
    log_post(LOG_LEVEL_INFO, "WDT", "Register task: %s", task_name);
    return ESP_OK;
}


esp_err_t watchdog_feed(void) {
    // In a real implementation, call esp_task_wdt_reset() here.
    return ESP_OK;
}