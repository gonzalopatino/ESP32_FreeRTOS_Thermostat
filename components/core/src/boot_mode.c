/**
 * @file boot_mode.c
 * @brief Boot mode state machine implementation.
 *
 * Determines whether device should boot into normal thermostat mode
 * or setup/provisioning mode.
 */

#include "core/boot_mode.h"
#include "core/provisioning.h"
#include "core/config.h"
#include "core/logging.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

// -----------------------------------------------------------------------------
// Private State
// -----------------------------------------------------------------------------

static const char *TAG = "BOOT";

// Current boot mode (set during determination)
static boot_mode_t s_current_mode = BOOT_MODE_UNKNOWN;

// -----------------------------------------------------------------------------
// Public API Implementation
// -----------------------------------------------------------------------------

boot_mode_t boot_mode_determine(void)
{
    // Check provisioning state from NVS
    bool is_provisioned = provisioning_is_provisioned();

    if (is_provisioned) {
        s_current_mode = BOOT_MODE_NORMAL;
        log_post(LOG_LEVEL_INFO, TAG, "Boot mode: NORMAL (device provisioned)");
    } else {
        s_current_mode = BOOT_MODE_SETUP;
        log_post(LOG_LEVEL_INFO, TAG, "Boot mode: SETUP (device not provisioned)");
    }

    return s_current_mode;
}

boot_mode_t boot_mode_get(void)
{
    return s_current_mode;
}

void boot_mode_force_setup(void)
{
    s_current_mode = BOOT_MODE_SETUP;
    log_post(LOG_LEVEL_WARN, TAG, "Boot mode FORCED to SETUP by user");
}

const char *boot_mode_to_str(boot_mode_t mode)
{
    switch (mode) {
        case BOOT_MODE_NORMAL:  return "NORMAL";
        case BOOT_MODE_SETUP:   return "SETUP";
        case BOOT_MODE_UNKNOWN:
        default:                return "UNKNOWN";
    }
}

bool boot_mode_check_button_held(uint32_t hold_ms)
{
    // Configure MODE button GPIO as input with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BTN_MODE),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Check if button is currently pressed (active low)
    if (gpio_get_level(GPIO_BTN_MODE) != 0) {
        // Button not pressed at boot
        return false;
    }

    printf("[BOOT] MODE button detected - hold for %lu ms to enter Setup Mode...\n",
           (unsigned long)hold_ms);

    // Button is pressed - wait and check if held for required duration
    const uint32_t check_interval_ms = 100;
    uint32_t elapsed_ms = 0;

    while (elapsed_ms < hold_ms) {
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed_ms += check_interval_ms;

        // Check if button was released
        if (gpio_get_level(GPIO_BTN_MODE) != 0) {
            printf("[BOOT] MODE button released early - continuing normal boot\n");
            return false;
        }

        // Progress indicator
        if (elapsed_ms % 500 == 0) {
            printf("[BOOT] Still holding... %lu/%lu ms\n",
                   (unsigned long)elapsed_ms, (unsigned long)hold_ms);
        }
    }

    // Button held for full duration
    printf("[BOOT] MODE button held for %lu ms - entering Setup Mode!\n",
           (unsigned long)hold_ms);

    return true;
}
