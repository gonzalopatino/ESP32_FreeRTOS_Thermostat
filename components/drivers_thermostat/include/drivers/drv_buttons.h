#ifndef DRV_BUTTONS_H
#define DRV_BUTTONS_H

#include "core/app_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/error.h"

/**
 * @brief Logical button events produced by the driver.
 */
typedef enum {
    BUTTON_EVENT_UP = 0,
    BUTTON_EVENT_DOWN
} button_event_t;

/**
 * @brief Initialize button hardware and create event queue.
 *
 * Configures GPIOs, attaches interrupts, and creates an internal
 * FreeRTOS queue where ISR will post button_event_t values.
 */
app_error_t drv_buttons_init(void);

/**
 * @brief Get handle to the internal button event queue.
 *
 * The UI task will block on this queue and handle events.
 */
QueueHandle_t drv_buttons_get_queue(void);

#endif  // DRV_BUTTONS_H
