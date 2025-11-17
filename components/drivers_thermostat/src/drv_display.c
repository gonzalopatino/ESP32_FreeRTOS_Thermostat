#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include <string.h>

#include "drivers/drv_display.h"
#include "core/logging.h"
#include "core/error.h"
#include "core/config.h"

static const char *TAG = "LCD";

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
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);

    gpio_set_level(LCD_PIN_EN, 0);
    gpio_set_level(LCD_PIN_RS, 0);
}

static inline void lcd_pulse(void)
{
    gpio_set_level(LCD_PIN_EN, 1);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_PIN_EN, 0);
    esp_rom_delay_us(40);  // command settle time
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

    lcd_write_nibble((val >> 4) & 0x0F);
    lcd_write_nibble(val & 0x0F);

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
    esp_rom_delay_us(50000);

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

    // Entry mode
    lcd_cmd(0x06);

    // Display on
    lcd_cmd(0x0C);
}

/* ---------------- API ---------------- */

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    static const uint8_t row_addr[] = {0x00, 0x40};
    lcd_cmd(0x80 | (row_addr[row] + col));
}

static void lcd_write_string(const char *s)
{
    while (*s) lcd_data(*s++);
}

app_error_t drv_display_init(void)
{
    lcd_gpio_init();
    lcd_init_sequence();

    log_post(LOG_LEVEL_INFO, TAG, "LCD initialized");
    return ERR_OK;
}

app_error_t drv_display_show_state(const thermostat_state_t *s)
{
    (void)s;

    lcd_cmd(0x01);

    lcd_set_cursor(0, 0);
    lcd_write_string("ESP32 HD44780 OK");

    lcd_set_cursor(1, 0);
    lcd_write_string("Hello, GonzA!");

    return ERR_OK;
}
