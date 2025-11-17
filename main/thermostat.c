#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define HEARTBEAT_GPIO GPIO_NUM_2   // Change this if your LED is on a different pin

static const char *TAG = "HEARTBEAT";

static void heartbeat_task(void *arg)
{
    // Configure the LED pin as output
    gpio_reset_pin(HEARTBEAT_GPIO);
    gpio_set_direction(HEARTBEAT_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(HEARTBEAT_GPIO, 1);
        ESP_LOGI(TAG, "Heartbeat ON");
        vTaskDelay(pdMS_TO_TICKS(500));

        gpio_set_level(HEARTBEAT_GPIO, 0);
        ESP_LOGI(TAG, "Heartbeat OFF");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting heartbeat task...");
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 5, NULL);
}
