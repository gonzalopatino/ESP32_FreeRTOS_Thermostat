#include "core/thermostat_config.h"
#include "core/config.h"
#include "core/logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "TH_CFG";

// Internal configuration object, not exposed directly.
static thermostat_config_t s_cfg;

// Mutex to protect s_cfg from concurrent access.
static SemaphoreHandle_t s_cfg_mutex = NULL;

app_error_t thermostat_config_init(void)
{
    // Create mutex once.
    s_cfg_mutex = xSemaphoreCreateMutex();
    if (s_cfg_mutex == NULL) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to create config mutex");
        return ERR_GENERIC;
    }

    // Load defaults from config.h
    s_cfg.setpoint_c   = THERMOSTAT_SETPOINT_C;
    s_cfg.hysteresis_c = THERMOSTAT_HYSTERESIS_C;

    log_post(LOG_LEVEL_INFO, TAG,
             "Init setpoint=%.2fC hysteresis=%.2fC",
             s_cfg.setpoint_c, s_cfg.hysteresis_c);

    return ERR_OK;
}

app_error_t thermostat_config_get(thermostat_config_t *out_cfg)
{
    if (out_cfg == NULL) {
        return ERR_GENERIC;
    }

    if (s_cfg_mutex == NULL) {
        // Not initialized, this is a programming error.
        return ERR_GENERIC;
    }

    if (xSemaphoreTake(s_cfg_mutex, portMAX_DELAY) == pdTRUE) {
        *out_cfg = s_cfg;  // struct copy
        xSemaphoreGive(s_cfg_mutex);
        return ERR_OK;
    }

    return ERR_GENERIC;
}

app_error_t thermostat_config_set(const thermostat_config_t *new_cfg)
{
    if (new_cfg == NULL) {
        return ERR_GENERIC;
    }

    if (s_cfg_mutex == NULL) {
        return ERR_GENERIC;
    }

    if (xSemaphoreTake(s_cfg_mutex, portMAX_DELAY) == pdTRUE) {

        s_cfg = *new_cfg;  // struct copy

        log_post(LOG_LEVEL_INFO, TAG,
                 "Update setpoint=%.2fC hysteresis=%.2fC",
                 s_cfg.setpoint_c, s_cfg.hysteresis_c);

        xSemaphoreGive(s_cfg_mutex);
        return ERR_OK;
    }

    return ERR_GENERIC;
}
