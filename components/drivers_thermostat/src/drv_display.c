#include "drivers/drv_display.h"

#include "core/config.h"    // LCD_PIN_RS, LCD_PIN_EN, LCD_PIN_D4..D7
#include "core/logging.h"   // log_post
#include "core/error.h"     // ERR_OK, ERR_GENERIC

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "LCD";

static bool s_lcd_initialized = false;

/* ---------------- Low level helpers ---------------- */

static void lcd_gpio_init(void)
{
    uint64_t mask =
        (1ULL << LCD_PIN_RS) |
        (1ULL << LCD_PIN_EN) |
        (1ULL << LCD_PIN_D4) |
        (1ULL << LCD_PIN_D5) |
        (1ULL << LCD_PIN_D6) |
        (1ULL << LCD_PIN_D7);

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Known idle state
    gpio_set_level(LCD_PIN_RS, 0);
    gpio_set_level(LCD_PIN_EN, 0);
    gpio_set_level(LCD_PIN_D4, 0);
    gpio_set_level(LCD_PIN_D5, 0);
    gpio_set_level(LCD_PIN_D6, 0);
    gpio_set_level(LCD_PIN_D7, 0);
}

static void lcd_pulse_enable(void)
{
    // EN high then low, with short delays so the LCD latches data
    gpio_set_level(LCD_PIN_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(LCD_PIN_EN, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_write_nibble(uint8_t nibble)
{
    gpio_set_level(LCD_PIN_D4, (nibble >> 0) & 0x1);
    gpio_set_level(LCD_PIN_D5, (nibble >> 1) & 0x1);
    gpio_set_level(LCD_PIN_D6, (nibble >> 2) & 0x1);
    gpio_set_level(LCD_PIN_D7, (nibble >> 3) & 0x1);

    lcd_pulse_enable();
}

static void lcd_write_byte(uint8_t value, bool is_data)
{
    gpio_set_level(LCD_PIN_RS, is_data ? 1 : 0);

    // High nibble then low nibble
    lcd_write_nibble((value >> 4) & 0x0F);
    lcd_write_nibble(value & 0x0F);

    // Generic instruction delay
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void lcd_write_cmd(uint8_t cmd)
{
    lcd_write_byte(cmd, false);
}

static void lcd_write_data(uint8_t data)
{
    lcd_write_byte(data, true);
}

/* ---------------- Public API ---------------- */

app_error_t drv_display_init(void)
{
    if (s_lcd_initialized) {
        return ERR_OK;
    }

    lcd_gpio_init();

    // Power on wait
    vTaskDelay(pdMS_TO_TICKS(50));

    // Init sequence for HD44780 in 4 bit mode

    // 1. Function set in 8 bit mode, three times
    lcd_write_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 2. Switch to 4 bit
    lcd_write_nibble(0x02);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 3. Function set: 4 bit, 2 lines, 5x8 dots
    lcd_write_cmd(0x28);

    // 4. Display off
    lcd_write_cmd(0x08);

    // 5. Clear display
    lcd_write_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));

    // 6. Entry mode set: increment, no shift
    lcd_write_cmd(0x06);

    // 7. Display on, cursor off, blink off
    lcd_write_cmd(0x0C);

    s_lcd_initialized = true;
    log_post(LOG_LEVEL_INFO, TAG, "LCD initialized");

    return ERR_OK;
}

app_error_t drv_display_clear(void)
{
    if (!s_lcd_initialized) {
        return ERR_GENERIC;
    }

    lcd_write_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
    return ERR_OK;
}

app_error_t drv_display_write_line(uint8_t row, const char *text)
{
    if (!s_lcd_initialized) {
        return ERR_GENERIC;
    }

    if (row >= LCD_ROWS) {
        return ERR_GENERIC;
    }

    // Base DDRAM address for each row on a 16x2
    uint8_t base_addr = (row == 0) ? 0x00 : 0x40;

    char buf[LCD_COLS];
    size_t i = 0;

    // Copy up to LCD_COLS chars
    while (i < LCD_COLS && text && text[i] != '\0') {
        buf[i] = text[i];
        i++;
    }
    // Pad the rest with spaces
    while (i < LCD_COLS) {
        buf[i++] = ' ';
    }

    // Set DDRAM address to start of row
    lcd_write_cmd(0x80 | base_addr);

    // Write exactly LCD_COLS characters
    for (size_t col = 0; col < LCD_COLS; ++col) {
        lcd_write_data((uint8_t)buf[col]);
    }

    return ERR_OK;
}

app_error_t drv_display_show_state(const thermostat_state_t *state)
{
    if (!s_lcd_initialized || state == NULL) {
        return ERR_GENERIC;
    }

    char line0[LCD_COLS + 1];
    char line1[LCD_COLS + 1];

    // Line 0: indoor and outdoor temps
    snprintf(line0, sizeof(line0),
             "In:%2.1f Out:%2.1f",
             state->tin_c,
             state->tout_c);

    const char *out_str =
        (state->output == THERMOSTAT_OUTPUT_HEAT_ON) ? "ON" : "OFF";

    // Line 1: setpoint, hysteresis, output
    snprintf(line1, sizeof(line1),
             "Sp:%2.1f H:%1.1f %s",
             state->setpoint_c,
             state->hysteresis_c,
             out_str);

    // Log exactly what we intend to show on LCD
    log_post(LOG_LEVEL_INFO, TAG,
             "LCD lines -> \"%.*s\" | \"%.*s\"",
             LCD_COLS, line0,
             LCD_COLS, line1);

    drv_display_write_line(0, line0);
    drv_display_write_line(1, line1);

    return ERR_OK;
}
