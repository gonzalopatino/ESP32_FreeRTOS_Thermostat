#include "drivers/drv_display.h"

#include "core/config.h"    // LCD_PIN_RS, LCD_PIN_EN, LCD_PIN_D4..D7, LCD_ROWS, LCD_COLS
#include "core/logging.h"   // log_post
#include "core/error.h"     // app_error_t, ERR_OK, ERR_GENERIC
#include "core/thermostat.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "LCD";

static bool s_lcd_initialized = false;

/* ---------------- Low-Level GPIO Helpers ---------------- */

static void lcd_gpio_init(void)
{
    uint64_t mask =
        (1ULL << LCD_PIN_RS) |
        (1ULL << LCD_PIN_EN) |
        (1ULL << LCD_PIN_D4) |
        (1ULL << LCD_PIN_D5) |
        (1ULL << LCD_PIN_D6) |
        (1ULL << LCD_PIN_D7);

    gpio_config_t cfg = {
        .pin_bit_mask = mask,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);

    // Known idle state: EN low, RS low. Data lines will be driven as needed.
    gpio_set_level(LCD_PIN_EN, 0);
    gpio_set_level(LCD_PIN_RS, 0);
}

static inline void lcd_pulse(void)
{
    // Short enable pulse using ROM microsecond delay
    gpio_set_level(LCD_PIN_EN, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_PIN_EN, 0);
    esp_rom_delay_us(40);   // command settle time
}

static inline void lcd_write_nibble(uint8_t nib)
{
    gpio_set_level(LCD_PIN_D4, (nib >> 0) & 1);
    gpio_set_level(LCD_PIN_D5, (nib >> 1) & 1);
    gpio_set_level(LCD_PIN_D6, (nib >> 2) & 1);
    gpio_set_level(LCD_PIN_D7, (nib >> 3) & 1);

    lcd_pulse();
}

static void lcd_send(uint8_t val, bool rs)
{
    gpio_set_level(LCD_PIN_RS, rs ? 1 : 0);

    // High nibble then low nibble
    lcd_write_nibble((val >> 4) & 0x0F);
    lcd_write_nibble(val & 0x0F);

    // Clear (0x01) and home (0x02) need a longer delay
    if (val == 0x01 || val == 0x02) {
        esp_rom_delay_us(2000);
    } else {
        esp_rom_delay_us(50);
    }
}

static inline void lcd_cmd(uint8_t cmd)  { lcd_send(cmd, false); }
static inline void lcd_data(uint8_t ch)  { lcd_send(ch, true); }

/* ---------------- LCD Init Sequence ---------------- */

static void lcd_init_sequence(void)
{
    // Wait for power to stabilize
    esp_rom_delay_us(50000);   // 50 ms

    gpio_set_level(LCD_PIN_RS, 0);

    // Force 8-bit mode (3 times)
    lcd_write_nibble(0x03); esp_rom_delay_us(4500);
    lcd_write_nibble(0x03); esp_rom_delay_us(4500);
    lcd_write_nibble(0x03); esp_rom_delay_us(150);

    // Switch to 4-bit
    lcd_write_nibble(0x02);
    esp_rom_delay_us(150);

    // Function set: 4-bit, 2-line, 5x8
    lcd_cmd(0x28);

    // Display off
    lcd_cmd(0x08);

    // Clear
    lcd_cmd(0x01);

    // Entry mode: increment, no shift
    lcd_cmd(0x06);

    // Display on, cursor off, blink off
    lcd_cmd(0x0C);
}

/* ---------------- Positioning & String helpers ---------------- */

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    static const uint8_t row_addr[] = {0x00, 0x40};  // 16x2 DDRAM bases

    if (row >= LCD_ROWS) {
        row = 0;  // fallback
    }
    lcd_cmd(0x80 | (row_addr[row] + col));
}

/* ---------------- Public API ---------------- */

app_error_t drv_display_init(void)
{
    if (s_lcd_initialized) {
        return ERR_OK;
    }

    lcd_gpio_init();
    lcd_init_sequence();

    s_lcd_initialized = true;
    log_post(LOG_LEVEL_INFO, TAG, "LCD initialized (ROM-delay driver)");

    return ERR_OK;
}

app_error_t drv_display_clear(void)
{
    if (!s_lcd_initialized) {
        return ERR_GENERIC;
    }

    lcd_cmd(0x01);
    // lcd_send already includes the long delay for 0x01, but we keep code explicit
    esp_rom_delay_us(2000);
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

    // Move cursor to start of the row
    lcd_set_cursor(row, 0);

    // Write up to LCD_COLS chars, then pad with spaces
    size_t i = 0;

    while (i < LCD_COLS && text && text[i] != '\0') {
        lcd_data((uint8_t)text[i]);
        i++;
    }
    while (i < LCD_COLS) {
        lcd_data(' ');
        i++;
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

           

    // Map mode to short 1-letter label.
    char mode_char = 'O';  // Off
    switch (state->mode) {
    case THERMOSTAT_MODE_HEAT: mode_char = 'H'; break;
    case THERMOSTAT_MODE_COOL: mode_char = 'C'; break;
    case THERMOSTAT_MODE_AUTO: mode_char = 'A'; break;
    case THERMOSTAT_MODE_OFF:
    default:
        mode_char = 'O';
        break;
    }

    // Map output to short string.
    const char *out_str = "OFF";
    if (state->output == THERMOSTAT_OUTPUT_HEAT_ON) {
        out_str = "HOn";
    } else if (state->output == THERMOSTAT_OUTPUT_COOL_ON) {
        out_str = "COn";
    }
    


    // Line 1: setpoint, hysteresis, mode / output
    // Example: "Sp:22 H:0.5 HOn"
    snprintf(line1, sizeof(line1),
             "Sp:%2.0f H:%1.1f %c%s",
             state->setpoint_c,
             state->hysteresis_c,
             mode_char,
             out_str);

    // Log exactly what we intend to show on LCD (truncated to LCD_COLS for safety).
    log_post(LOG_LEVEL_INFO, TAG,
             "LCD lines -> \"%.*s\" | \"%.*s\"",
             LCD_COLS, line0,
             LCD_COLS, line1);

    drv_display_write_line(0, line0);
    drv_display_write_line(1, line1);

    return ERR_OK;
}
