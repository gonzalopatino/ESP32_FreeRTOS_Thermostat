/**
 * @file    task_heartbeat.c
 * @brief   System heartbeat LED task.
 *
 * Blinks the onboard LED to indicate system health and
 * provides visual confirmation that the firmware is running.
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

/* ═══════════════════════════════════════════════════════════════════════════
 * INCLUDES
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "core/config.h"
#include "core/logging.h"
#include "core/watchdog.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * PRIVATE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize the heartbeat LED GPIO.
 */
static void heartbeat_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Guarantee LED starts in a known OFF state
    gpio_set_level (LED_GPIO, 0);
}


/**
 * @brief FreeRTOS heartbeat task.
 *
 * This task:
 *  - blinks an LED every 1 second
 *  - logs a "system alive" message
 *  - feeds the watchdog so the system doesn't reset
 *
 * It runs forever at the priority assigned in config.h.
 */


static void task_heartbeat(void *arg) {
    (void)arg;        // Unused, but avoids compiler warnings

    int counter = 0;    // Counts how many iterations we've run
    int led_state = 0;  // Tracks LED ON/OFF state

    // Initialize hardware once inside the task
    heartbeat_led_init();
    
    // Register this task with the watchdog system, so it must periodically call watchdog_feed()
    watchdog_register_current("HEARTBEAT");

    while (1) {
        // Toggle LED state
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);


        // Post structured Log message to the Logger queue
        log_post(LOG_LEVEL_INFO, 
                "HEARTBEAT",
                 "System alive, counter=%d, led_state=%d",  
                 counter++,led_state);
        
        // Tell the watchdog we are alive
        watchdog_feed();
        
        // Sleep for 1000 ms before repeating
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Spawn the heartbeat task.
 *
 * Called once from main (or wherever your application initializes tasks).
 * Stack size and priority come from config.h so they can be tuned centrally.
 */

void task_heartbeat_start(void) {
    xTaskCreate(
        task_heartbeat,             // Task entry function
        "task_heartbeat",           // Task name (for debugging)
        TASK_STACK_HEARTBEAT,       // Stack size in words
        NULL,                       // No parameters passed
        TASK_PRIO_HEARTBEAT,        // Task priority
        NULL                        // Not storing task handle
    );
}
