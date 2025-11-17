#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/config.h"
#include "core/logging.h"
#include "driver/gpio.h"




static void heartbeat_led_init(void) {
    // Configure LED pin as output
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // start LED off
    gpio_set_level (LED_GPIO, 0);
}


static void task_heartbeat(void *arg) {
    
    (void)arg;

    int counter = 0;
    int led_state = 0;

    // Initialize LED once in the task context
    heartbeat_led_init();

    while (1) {
        // Toggle LED
        led_state = !led_state;
        gpio_set_level(LED_GPIO, led_state);


        // Log status
        log_post(LOG_LEVEL_INFO, "HEARTBEAT",
                 "System alive, counter=%d, led_state=%d",  counter++, led_state);
                 
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void task_heartbeat_start(void) {
    xTaskCreate(
        task_heartbeat,
        "task_heartbeat",
        TASK_STACK_HEARTBEAT,
        NULL,
        TASK_PRIO_HEARTBEAT,
        NULL
    );
}
