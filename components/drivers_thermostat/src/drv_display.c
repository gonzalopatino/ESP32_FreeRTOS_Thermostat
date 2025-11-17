#include "drivers/drv_display.h"
#include "core/logging.h"
#include "core/error.h"
#include "core/config.h"
#include "core/thermostat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "DRV_DISPLAY";

/* ---------------- Hardware helpers (4-bit HD44780) ---------------- */

/**
 * @brief Set RS and EN to known idle state and configure all LCD pins as outputs.
 */
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

    // Idle states: EN low, RS low.
    gpio_set_level(LCD_PIN_EN, 0);
    gpio_set_level(LCD_PIN_RS, 0);
}

/**
 * @brief Small delay helper in milliseconds.
 */
static void lcd_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Pulse the EN line to latch current data lines into the LCD.
 */
static void lcd_pulse_enable(void)
{
    gpio_set_level(LCD_PIN_EN, 1);
    // Enable pulse width: a few microseconds are enough; 1 ms is safe
    lcd_delay_ms(1);
    gpio_set_level(LCD_PIN_EN, 0);
    lcd_delay_ms(1);
}

/**
 * @brief Put a 4-bit nibble on D4..D7.
 */
static void lcd_write_nibble(uint8_t nibble)
{
    gpio_set_level(LCD_PIN_D4, (nibble >> 0) & 0x01);
    gpio_set_level(LCD_PIN_D5, (nibble >> 1) & 0x01);
    gpio_set_level(LCD_PIN_D6, (nibble >> 2) & 0x01);
    gpio_set_level(LCD_PIN_D7, (nibble >> 3) & 0x01);

    lcd_pulse_enable();
}

/**
 * @brief Send a full 8-bit value (command or data) in two 4-bit transfers.
 *
 * @param value  8-bit command or character.
 * @param rs     0 for command, 1 for data (character).
 */
static void lcd_send(uint8_t value, int rs)
{
    gpio_set_level(LCD_PIN_RS, rs ? 1 : 0);

    // High nibble first, then low nibble.
    lcd_write_nibble((value >> 4) & 0x0F);
    lcd_write_nibble(value & 0x0F);
}

/* ---------------- HD44780 basic commands ---------------- */

static void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0);
    // Most commands are done in < 2 ms; clear/home are slower.
    if (cmd == 0x01 || cmd == 0x02) {
        lcd_delay_ms(2);
    } else {
        lcd_delay_ms(1);
    }
}

static void lcd_write_char(char c)
{
    lcd_send((uint8_t)c, 1);
}

static void lcd_clear(void)
{
    lcd_command(0x01);  // Clear display
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    // For a 16x2, DDRAM base addresses are commonly:
    // row 0: 0x00, row 1: 0x40.
    static const uint8_t row_offsets[] = { 0x00, 0x40 };
    if (row > 1) {
        row = 1; // clamp
    }
    uint8_t addr = row_offsets[row] + col;
    lcd_command(0x80 | addr);  // Set DDRAM address
}

/**
 * @brief Write a null-terminated string to the current cursor position.
 */
static void lcd_write_string(const char *s)
{
    while (*s) {
        lcd_write_char(*s++);
    }
}

/**
 * @brief Initialize the HD44780 LCD in 4-bit mode.
 *
 * Sequence follows the standard 4-bit init flow. Timings are conservative
 * (using millisecond delays) to keep things simple and robust.
 */
static void lcd_init_sequence(void)
{
    // Wait for LCD power-up
    lcd_delay_ms(50);

    // Force into 8-bit mode three times, as per datasheet.
    gpio_set_level(LCD_PIN_RS, 0);

    lcd_write_nibble(0x03);
    lcd_delay_ms(5);

    lcd_write_nibble(0x03);
    lcd_delay_ms(5);

    lcd_write_nibble(0x03);
    lcd_delay_ms(5);

    // Switch to 4-bit mode
    lcd_write_nibble(0x02);
    lcd_delay_ms(5);

    // Function set: 4-bit, 2 lines, 5x8 font
    lcd_command(0x28);

    // Display off
    lcd_command(0x08);

    // Clear display
    lcd_clear();

    // Entry mode set: increment, no shift
    lcd_command(0x06);

    // Display on, cursor off, blink off
    lcd_command(0x0C);
}

/* ---------------- Public driver API ---------------- */

app_error_t drv_display_init(void)
{
    lcd_gpio_init();
    lcd_init_sequence();

    log_post(LOG_LEVEL_INFO, TAG, "HD44780 16x2 display initialized (4-bit mode)");
    return ERR_OK;
}

app_error_t drv_display_show_state(const thermostat_state_t *state)
{
    if (state == NULL) {
        return ERR_GENERIC;
    }

    char line1[17] = {0};  // 16 chars + null
    char line2[17]= {0};

    // First line: inside temp and setpoint, for example:
    // "Tin:22.3C Sp:22.0"
    snprintf(line1, sizeof(line1),
             "Tin:%2.0f Sp:%2.0f",
             state->tin_c,
             state->setpoint_c);

    // Second line: outside temp + output state:
    // "Tout:10.5C HEAT "
    const char *out_str =
        (state->output == THERMOSTAT_OUTPUT_HEAT_ON) ? "HEAT" : "IDLE";

    snprintf(line2, sizeof(line2),
             "Tout:%2.0f C:%s",
             state->tout_c,
             out_str);

    // Push lines to LCD
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_write_string(line1);

    lcd_set_cursor(1, 0);
    lcd_write_string(line2);

    // Optional: also log for debugging
    log_post(LOG_LEVEL_DEBUG, TAG,
             "LCD: \"%s\" / \"%s\"", line1, line2);

    return ERR_OK;
}
