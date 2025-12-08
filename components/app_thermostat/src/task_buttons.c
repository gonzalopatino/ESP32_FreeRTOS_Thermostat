#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_system.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"
#include "core/error.h"
#include "core/provisioning.h"

#include "core/thermostat_config.h"
#include "drivers/drv_buttons.h"
#include "drivers/drv_display.h"
#include "core/thermostat.h" // <-- for thermostat_get_mode / thermostat_set_mode

#include "app/task_buttons.h"
#include "app/setup_server.h"

#include <string.h>                 // strlen

static const char *TAG = "BTN_UI";

// Long-press threshold for settings mode (5 seconds)
#define SETTINGS_MODE_HOLD_MS  5000



/* ---------------- Setpoint helper ---------------- */

static void apply_setpoint_delta(float delta_c)
{
    thermostat_config_t cfg;
    if (thermostat_config_get(&cfg) != APP_ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_get");
        return;
    }

    cfg.setpoint_c += delta_c;

    if (thermostat_config_get(&cfg) != APP_ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_get");
        return;
    }

    cfg.setpoint_c += delta_c;

    if (cfg.setpoint_c < THERMOSTAT_SP_MIN_C) {
        cfg.setpoint_c = THERMOSTAT_SP_MIN_C;
    } else if (cfg.setpoint_c > THERMOSTAT_SP_MAX_C) {
        cfg.setpoint_c = THERMOSTAT_SP_MAX_C;
    }

    if (thermostat_config_set(&cfg) != APP_ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_config_set");
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG,
             "Setpoint changed to %.1f C (delta=%.1f)",
             cfg.setpoint_c, delta_c);
}

/**
 * @brief Cycle mode: HEAT -> COOL -> OFF -> HEAT ...
 */
static const char *mode_to_str(thermostat_mode_t mode)
{
    switch (mode) {
    case THERMOSTAT_MODE_OFF:  return "OFF";
    case THERMOSTAT_MODE_HEAT: return "HEAT";
    case THERMOSTAT_MODE_COOL: return "COOL";
    case THERMOSTAT_MODE_AUTO: return "AUTO";
    default:                   return "UNKNOWN";
    }
}
 
static void cycle_mode(void)
{
    thermostat_mode_t current;
    if (thermostat_get_mode(&current) != APP_ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_get_mode");
        return;
    }

    thermostat_mode_t next = THERMOSTAT_MODE_HEAT;

    switch (current) {
    case THERMOSTAT_MODE_HEAT:
        next = THERMOSTAT_MODE_COOL;
        break;
    case THERMOSTAT_MODE_COOL:
        next = THERMOSTAT_MODE_OFF;
        break;
    case THERMOSTAT_MODE_OFF:
        next = THERMOSTAT_MODE_AUTO;
        break;
    case THERMOSTAT_MODE_AUTO:
        next = THERMOSTAT_MODE_HEAT;
        break;
    default:
        next = THERMOSTAT_MODE_HEAT;
        break;
    }

    if (thermostat_set_mode(next) != APP_ERR_OK) {
        error_report(ERR_GENERIC, "thermostat_set_mode");
        return;
    }

    log_post(LOG_LEVEL_INFO, TAG,
             "Mode changed: %s -> %s",
             mode_to_str(current),
             mode_to_str(next));
}

/**
 * @brief Check if MODE button is being held for settings mode.
 * 
 * Called when MODE button press is detected. Monitors if button
 * stays pressed for SETTINGS_MODE_HOLD_MS milliseconds.
 * 
 * @return true if long press detected (settings mode should activate)
 */
static bool check_mode_long_press(void)
{
    const uint32_t check_interval_ms = 100;
    uint32_t elapsed_ms = 0;
    int countdown = 5;
    
    // Show initial message
    drv_display_write_line(0, "Hold for setup:");
    drv_display_write_line(1, "   5 seconds   ");
    
    while (elapsed_ms < SETTINGS_MODE_HOLD_MS) {
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_ms += check_interval_ms;
        
        // Check if button was released (GPIO high = released)
        if (gpio_get_level(GPIO_BTN_MODE) != 0) {
            // Button released - just do normal mode cycle
            return false;
        }
        
        // Update countdown display every second
        int new_countdown = (SETTINGS_MODE_HOLD_MS - elapsed_ms) / 1000 + 1;
        if (new_countdown != countdown && new_countdown > 0) {
            countdown = new_countdown;
            // Use static strings to avoid format truncation warnings
            static const char *countdown_msgs[] = {
                "   Settings 0   ",  // index 0 (unused)
                "   Settings 1   ",  // 1 second
                "   Settings 2   ",  // 2 seconds
                "   Settings 3   ",  // 3 seconds
                "   Settings 4   ",  // 4 seconds
                "   Settings 5   ",  // 5 seconds
            };
            int idx = (countdown > 5) ? 5 : countdown;
            drv_display_write_line(1, countdown_msgs[idx]);
        }
    }
    
    // Button held for full duration - settings mode!
    return true;
}

/**
 * @brief Enter settings mode directly.
 * 
 * Starts the settings AP and shows IP on LCD. Waits for any button press to exit.
 */
static void enter_settings_mode(void)
{
    log_post(LOG_LEVEL_INFO, TAG, "Entering Settings Mode...");
    
    drv_display_clear();
    drv_display_write_line(0, "Settings Mode");
    drv_display_write_line(1, "Starting AP...");
    
    // Start settings server (AP + STA mode)
    app_error_t err = setup_server_start_settings(NULL);
    if (err != APP_ERR_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to start settings mode!");
        drv_display_clear();
        drv_display_write_line(0, "Settings Error!");
        drv_display_write_line(1, "Try again");
        vTaskDelay(pdMS_TO_TICKS(3000));
        return;
    }
    
    // Get AP SSID and PIN
    char ap_ssid[33];
    setup_server_get_ssid(ap_ssid, sizeof(ap_ssid));
    const char *pin = setup_server_get_pin();
    
    // Show on LCD: line 1 = short SSID, line 2 = AP IP
    drv_display_clear();
    const char *short_ssid = (strlen(ap_ssid) > 10) ? (ap_ssid + 10) : ap_ssid;
    drv_display_write_line(0, short_ssid);
    drv_display_write_line(1, "192.168.4.1");
    
    log_post(LOG_LEVEL_INFO, TAG, "Settings AP: %s, PIN: %s, IP: 192.168.4.1", ap_ssid, pin);
    
    // Clear button queue to ignore any pending events
    QueueHandle_t q = drv_buttons_get_queue();
    button_event_t evt;
    while (xQueueReceive(q, &evt, 0) == pdTRUE) {
        // Discard any queued events
    }
    
    // Wait for any button press to exit settings mode
    log_post(LOG_LEVEL_INFO, TAG, "Press any button to exit settings mode");
    while (1) {
        if (xQueueReceive(q, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            log_post(LOG_LEVEL_INFO, TAG, "Button pressed - exiting settings mode");
            break;
        }
        watchdog_feed();
    }
    
    // Stop settings mode and restart
    log_post(LOG_LEVEL_INFO, TAG, "Stopping settings mode, restarting...");
    drv_display_clear();
    drv_display_write_line(0, "Exiting...");
    drv_display_write_line(1, "Restarting");
    
    setup_server_stop_settings();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

/* ---------------- Task ---------------- */

/**
 * @brief Task that consumes button events and adjusts thermostat setpoint / mode.
 *
 * Uses a simple time-based debounce in task context.
 */
static void task_buttons(void *arg)
{
    (void)arg;

    watchdog_register_current("BUTTONS");

    if (drv_buttons_init() != APP_ERR_OK) {
        error_fatal(ERR_GENERIC, "drv_buttons_init");
    }

    QueueHandle_t q = drv_buttons_get_queue();
    if (q == NULL) {
        error_fatal(ERR_GENERIC, "drv_buttons_get_queue");
    }

     // Debounce: remember last accepted event time per button
    TickType_t last_up_ticks   = 0;
    TickType_t last_down_ticks = 0;
    TickType_t last_mode_ticks = 0;

    const TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);

    while (1) {
        button_event_t evt;
        if (xQueueReceive(q, &evt, portMAX_DELAY) == pdTRUE) {
            TickType_t now = xTaskGetTickCount();

            switch (evt) {
            case BUTTON_EVENT_UP:
                if ((now - last_up_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(+THERMOSTAT_SP_STEP_C);
                    last_up_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "UP ignored (debounce)");
                }
                break;

            case BUTTON_EVENT_DOWN:
                if ((now - last_down_ticks) >= debounce_ticks) {
                    apply_setpoint_delta(-THERMOSTAT_SP_STEP_C);
                    last_down_ticks = now;
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "DOWN ignored (debounce)");
                }
                break;

            case BUTTON_EVENT_MODE:
                if ((now - last_mode_ticks) >= debounce_ticks) {
                    // Check for long press (settings mode)
                    if (check_mode_long_press()) {
                        // Long press detected - enter settings mode directly
                        enter_settings_mode();
                    } else {
                        // Short press - cycle mode
                        cycle_mode();
                    }
                    last_mode_ticks = xTaskGetTickCount();  // Update after potential long wait
                } else {
                    log_post(LOG_LEVEL_DEBUG, TAG, "MODE ignored (debounce)");
                }
                break;

            default:
                // Unknown event, ignore
                break;
            }


            watchdog_feed();
        }
    }
}

void task_buttons_start(void)
{
    xTaskCreate(
        task_buttons,
        "task_buttons",
        TASK_STACK_BUTTONS,
        NULL,
        TASK_PRIO_BUTTONS,
        NULL
    );
}
