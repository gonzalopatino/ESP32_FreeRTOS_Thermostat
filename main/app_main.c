/**
 * @file    app_main.c
 * @brief   Application entry point and boot mode orchestration.
 *
 * This module serves as the main entry point for the ThermostatRTOS firmware.
 * It handles:
 *   - System initialization (NVS, WiFi, logging)
 *   - Boot mode detection (NORMAL vs SETUP)
 *   - Task orchestration and startup sequence
 *   - Provisioning flow for first-time device setup
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * @license   MIT License - See LICENSE file for details.
 *
 * SPDX-License-Identifier: MIT
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * INCLUDES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Core system includes */
#include "core/config.h"
#include "core/logging.h"
#include "core/error.h"
#include "core/watchdog.h"
#include "core/provisioning.h"
#include "core/boot_mode.h"
#include "core/thermostat.h"

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Application task includes */
#include "app/task_common.h"
#include "app/task_sensors.h"
#include "app/task_logger.h"
#include "app/task_heartbeat.h"
#include "app/task_control.h"
#include "app/task_display.h"
#include "app/task_buttons.h"
#include "app/task_net.h"
#include "app/setup_server.h"

/* Driver includes */
#include "drivers/drv_display.h"

/* Standard library includes */
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Duration in ms to hold MODE button for manual setup entry */
#define SETUP_BUTTON_HOLD_MS  3000

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Start normal thermostat operation.
 *
 * Called when device is provisioned and ready for normal use.
 * Initializes all thermostat tasks.
 */
static void start_normal_mode(void) {
    printf("[APP] Starting NORMAL mode - thermostat operation\n");

    // Create common queues used for inter-task communication.
    tasks_common_init_queues();

    // Initialize thermostat core (which also initializes thermostat_config
    // and loads setpoint + hysteresis from defaults or stored values).
    if (thermostat_core_init() != APP_ERR_OK) {
        error_fatal(ERR_GENERIC, "thermostat_core_init");
    }

    // Start NET (Wi-Fi) before any task that might need connectivity.
    task_net_start();

    // Emit startup message with application name and version.
    log_post(LOG_LEVEL_INFO, "APP",
             "%s v%s starting", APP_NAME, APP_FW_VERSION);

    // ------------------------------
    //      Task Startup Order
    // ------------------------------

    // 1. Start the logger task FIRST.
    task_logger_start();

    // 2. Start the sensor task.
    task_sensors_start();

    // 3. Start the control task (consumes samples + drives heater GPIO).
    task_control_start();

    // 4. Buttons start
    task_buttons_start();

    // 5. Start the display task
    task_display_start();

    // 6. Start the heartbeat task last.
    task_heartbeat_start();

    thermostat_set_mode(THERMOSTAT_MODE_AUTO);
    
    log_post(LOG_LEVEL_INFO, "APP", "Normal mode running - hold MODE 5s for settings");
    
    // Note: Settings mode is handled directly by button task (long press on MODE)
    // Main task can just idle or do other work
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Start setup/provisioning mode.
 *
 * Called when device needs first-time setup or user forced setup mode.
 * Will start softAP and HTTP server for the setup wizard.
 */
static void start_setup_mode(void) {
    printf("[APP] Starting SETUP mode - WiFi provisioning\n");

    // Start logger so we can see log messages
    task_logger_start();

    // Start the SoftAP and HTTP server
    app_error_t err = setup_server_start();
    if (err != APP_ERR_OK) {
        printf("[APP] ERROR: Failed to start setup server!\n");
        log_post(LOG_LEVEL_ERROR, "SETUP", "Failed to start setup server");
        
        // Show error on LCD
        if (drv_display_init() == APP_ERR_OK) {
            drv_display_clear();
            drv_display_write_line(0, "SETUP ERROR!");
            drv_display_write_line(1, "Restart device");
        }
        
        // Hang here - user needs to restart
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Get the SSID and PIN to display
    char ap_ssid[33];
    setup_server_get_ssid(ap_ssid, sizeof(ap_ssid));
    const char *pin = setup_server_get_pin();
    
    printf("============================================\n");
    printf("  FIRST-TIME SETUP READY\n");
    printf("  \n");
    printf("  1. Connect to WiFi: %s\n", ap_ssid);
    printf("  2. Browser will open automatically\n");
    printf("  3. Enter PIN: %s\n", pin);
    printf("============================================\n");

    // Initialize LCD and show SSID on line 1, PIN on line 2
    if (drv_display_init() == APP_ERR_OK) {
        drv_display_clear();
        
        // Line 1: Show shortened SSID (LCD is 16 chars wide)
        // SSID format: "ThinkSenseThermo-XXXX" (21 chars)
        // We'll show: "Thermo-XXXX" (11 chars) which fits in 16-char LCD
        char lcd_line1[17] = {0};
        size_t ap_len = strlen(ap_ssid);
        const char *short_ssid = (ap_len > 10) ? (ap_ssid + 10) : ap_ssid;
        strncpy(lcd_line1, short_ssid, sizeof(lcd_line1) - 1);
        drv_display_write_line(0, lcd_line1);
        
        // Line 2: Show PIN prominently (format: "PIN: XXXX")
        char lcd_line2[17];
        snprintf(lcd_line2, sizeof(lcd_line2), "PIN: %s", pin);
        drv_display_write_line(1, lcd_line2);
        
        log_post(LOG_LEVEL_INFO, "SETUP", "LCD: %s / %s", lcd_line1, lcd_line2);
    } else {
        log_post(LOG_LEVEL_ERROR, "SETUP", "LCD init failed");
    }

    log_post(LOG_LEVEL_INFO, "SETUP", "Setup server running - connect to %s", ap_ssid);
    log_post(LOG_LEVEL_INFO, "SETUP", "Security PIN: %s", pin);

    // Monitor for setup completion - poll for connect request
    while (1) {
        // Check if user has submitted WiFi credentials and requested connection
        if (setup_server_connect_pending()) {
            log_post(LOG_LEVEL_INFO, "SETUP", "WiFi connection requested, switching to STA mode...");
            
            // Show connecting status on LCD
            drv_display_clear();
            drv_display_write_line(0, "Connecting...");
            
            // Perform the WiFi mode switch
            char ip_addr[16] = {0};
            app_error_t wifi_err = setup_server_connect_wifi(ip_addr, sizeof(ip_addr));
            
            if (wifi_err == APP_ERR_OK) {
                log_post(LOG_LEVEL_INFO, "SETUP", "Connected! IP: %s", ip_addr);
                
                // Store IP for use in Step 3 page
                setup_server_set_device_ip(ip_addr);
                
                // Show IP address on LCD for user to find the device
                drv_display_clear();
                drv_display_write_line(0, "Open browser:");
                drv_display_write_line(1, ip_addr);
                
                printf("============================================\n");
                printf("  WiFi CONNECTED!\n");
                printf("  \n");
                printf("  Device IP: %s\n", ip_addr);
                printf("  \n");
                printf("  Open this IP in your browser to\n");
                printf("  complete API key setup.\n");
                printf("============================================\n");
                
                // Start HTTP server on STA for Step 3 (API key entry)
                if (setup_server_start_step3() != APP_ERR_OK) {
                    log_post(LOG_LEVEL_ERROR, "SETUP", "Failed to start Step 3 server");
                    drv_display_clear();
                    drv_display_write_line(0, "Server Error!");
                    drv_display_write_line(1, "Restart device");
                    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
                }
                
                // Monitor for API key completion
                while (!setup_server_api_key_complete()) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                
                // Mark provisioning as complete so next boot enters normal mode
                if (provisioning_mark_complete() != APP_ERR_OK) {
                    log_post(LOG_LEVEL_ERROR, "SETUP", "Failed to mark provisioning complete!");
                }
                
                // API key received - setup complete!
                log_post(LOG_LEVEL_INFO, "SETUP", "Setup complete! Rebooting to normal mode...");
                drv_display_clear();
                drv_display_write_line(0, "Setup Complete!");
                drv_display_write_line(1, "Rebooting...");
                
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_restart();
                
            } else {
                log_post(LOG_LEVEL_ERROR, "SETUP", "WiFi connection failed!");
                
                // Show error and wait for restart
                drv_display_clear();
                drv_display_write_line(0, "WiFi FAILED!");
                drv_display_write_line(1, "Restart device");
                
                while (1) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));  // Poll every 500ms
    }
}

/**
 * @brief Entry point of the application.
 *
 * app_main() is called once by the ESP-IDF runtime after boot.
 * This function performs global system initialization and determines
 * whether to enter normal thermostat mode or setup/provisioning mode.
 */
void app_main(void) {
    // Initialize logging subsystem and create the log queue.
    // Must be called early, before any task tries to log messages.
    logging_init();

    // Start watchdog system. If it fails (rare), abort immediately.
    if (watchdog_init() != ESP_OK) {
        error_fatal(ERR_WATCHDOG_INIT_FAILED, "watchdog_init");
    }

    // Initialize provisioning subsystem (NVS for credentials/state).
    if (provisioning_init() != APP_ERR_OK) {
        error_fatal(ERR_GENERIC, "provisioning_init");
    }

    // Check if MODE button is held at boot (manual setup entry)
    bool force_setup = boot_mode_check_button_held(SETUP_BUTTON_HOLD_MS);

    // Determine boot mode based on provisioning state
    boot_mode_t mode = boot_mode_determine();

    // Override to setup mode if button was held
    if (force_setup) {
        boot_mode_force_setup();
        mode = BOOT_MODE_SETUP;
    }

    // Display boot mode
    printf("\n");
    printf("***********************************\n");
    printf("*  BOOT MODE: %-18s *\n", boot_mode_to_str(mode));
    printf("***********************************\n");
    printf("\n");

    // Branch based on boot mode
    if (mode == BOOT_MODE_SETUP) {
        start_setup_mode();
    } else {
        start_normal_mode();
    }
}
