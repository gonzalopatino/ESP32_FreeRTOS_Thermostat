#include "drivers/drv_buttons.h"
#include "core/config.h"
#include "core/logging.h"
#include "core/error.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/gpio.h"

static const char *TAG = "DRV_BTN";

// Queue used to send button events from ISR to task context.
static QueueHandle_t s_btn_queue = NULL;

// Simple mapping from GPIO to event
static button_event_t gpio_to_event(gpio_num_t gpio)
{
    if (gpio == GPIO_BTN_UP) {
        return BUTTON_EVENT_UP;
    } else if (gpio == GPIO_BTN_DOWN) {
        return BUTTON_EVENT_DOWN;
    } else if (gpio == GPIO_BTN_MODE) {
        return BUTTON_EVENT_MODE;
    } else {
        // Fallback: treat as DOWN (should not happen if wiring is correct).
        return BUTTON_EVENT_DOWN;
    }
}

/**
 * @brief ISR for button GPIOs.
 *
 * Kept very small. It only pushes an event into the queue.
 */
static void IRAM_ATTR button_isr_handler(void *arg)
{
    gpio_num_t gpio = (gpio_num_t)(intptr_t)arg;
    button_event_t evt = gpio_to_event(gpio);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_btn_queue != NULL) {
        xQueueSendFromISR(s_btn_queue, &evt, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

app_error_t drv_buttons_init(void)
{
    // Create event queue once
    s_btn_queue = xQueueCreate(BUTTON_EVENT_QUEUE_LEN, sizeof(button_event_t));
    if (s_btn_queue == NULL) {
        log_post(LOG_LEVEL_ERROR, TAG, "Failed to create button queue");
        return ERR_GENERIC;
    }

    // Configure both pins as inputs with pull-ups and falling-edge interrupts
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << GPIO_BTN_UP)   |
            (1ULL << GPIO_BTN_DOWN) |
            (1ULL << GPIO_BTN_MODE),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE   // pressed -> line goes low
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG, "gpio_config failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    // Install ISR service once
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        // INVALID_STATE means it was already installed which is fine
        log_post(LOG_LEVEL_ERROR, TAG, "gpio_install_isr_service failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    // Attach ISR handlers for each button
    err = gpio_isr_handler_add(GPIO_BTN_UP,
                               button_isr_handler,
                               (void *)(intptr_t)GPIO_BTN_UP);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "gpio_isr_handler_add(UP) failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    err = gpio_isr_handler_add(GPIO_BTN_DOWN,
                               button_isr_handler,
                               (void *)(intptr_t)GPIO_BTN_DOWN);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "gpio_isr_handler_add(DOWN) failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    err = gpio_isr_handler_add(GPIO_BTN_MODE,
                               button_isr_handler,
                               (void *)(intptr_t)GPIO_BTN_MODE);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "gpio_isr_handler_add(MODE) failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    log_post(LOG_LEVEL_INFO, TAG,
             "Buttons initialized (UP=%d, DOWN=%d, MODE=%d)",
             (int)GPIO_BTN_UP,
             (int)GPIO_BTN_DOWN,
             (int)GPIO_BTN_MODE);

    return APP_ERR_OK;
}

QueueHandle_t drv_buttons_get_queue(void)
{
    return s_btn_queue;
}
