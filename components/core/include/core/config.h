#ifndef CONFIG_H
#define CONFIG_H

// -----------------------------------------------------------------------------
// Application identity
// -----------------------------------------------------------------------------
#define APP_NAME        "ThermostatRTOS"
#define APP_FW_VERSION  "0.1.0"

// -----------------------------------------------------------------------------
// Task priorities (higher number = higher priority)
// -----------------------------------------------------------------------------
#define TASK_PRIO_LOGGER      5
#define TASK_PRIO_SENSORS     4
#define TASK_PRIO_HEARTBEAT   3
#define TASK_PRIO_CONTROL     5   // Control is as important as logging

// -----------------------------------------------------------------------------
// Task stack sizes (in words, ESP-IDF expects words for xTaskCreate)
// -----------------------------------------------------------------------------
#define TASK_STACK_LOGGER     4096
#define TASK_STACK_SENSORS    4096
#define TASK_STACK_HEARTBEAT  4096
#define TASK_STACK_CONTROL    4096

// -----------------------------------------------------------------------------
// Periods (milliseconds)
// -----------------------------------------------------------------------------
#define PERIOD_LOGGER_MS      50    // how often logger wakes when idle
#define PERIOD_SENSORS_MS     500   // sensor sampling period

// -----------------------------------------------------------------------------
// Logging subsystem
// -----------------------------------------------------------------------------
#define LOG_BUFFER_LEN        256   // per-record message buffer size
#define LOG_QUEUE_LENGTH      32    // depth of the logging queue

// -----------------------------------------------------------------------------
// Thermostat control parameters
// -----------------------------------------------------------------------------
#define THERMOSTAT_SETPOINT_C       22.0f   // default target temp (°C)
#define THERMOSTAT_HYSTERESIS_C      0.5f   // +/- hysteresis band (°C)

// -----------------------------------------------------------------------------
// Board pins
// -----------------------------------------------------------------------------
#define LED_GPIO              2   // Heartbeat LED
#define GPIO_HEAT_OUTPUT      12   // Heater relay / output pin
#define GPIO_COOL_OUTPUT      33   // Heater relay / output pin

// -----------------------------------------------------------------------------
// LCD 16x2 (HD44780) in 4-bit mode 
// -----------------------------------------------------------------------------
// #define LCD_PIN_RS           32
// #define LCD_PIN_EN           33

// #define LCD_PIN_D4           25
// #define LCD_PIN_D5           26
// #define LCD_PIN_D6           27
// #define LCD_PIN_D7           14

#define LCD_PIN_RS           19
#define LCD_PIN_EN           18

#define LCD_PIN_D4           5
#define LCD_PIN_D5           17
#define LCD_PIN_D6           16
#define LCD_PIN_D7           4

// -----------------------------------------------------------------------------
// Buttons 
// -----------------------------------------------------------------------------
// Up/down buttons to adjust setpoint
#define GPIO_BTN_UP              25   // pick your actual pin
#define GPIO_BTN_DOWN            26   // pick your actual pin

// New: mode button to cycle HEAT / COOL / OFF
#define GPIO_BTN_MODE            27

// Queue length for button events
#define BUTTON_EVENT_QUEUE_LEN   8

// Setpoint adjustment step and limits
#define THERMOSTAT_SP_STEP_C     0.5f
#define THERMOSTAT_SP_MIN_C      15.0f
#define THERMOSTAT_SP_MAX_C      28.0f

// Button debounce time (in ms)
#define BUTTON_DEBOUNCE_MS       200

// Task for button handling
#define TASK_PRIO_BUTTONS        4
#define TASK_STACK_BUTTONS       4096

// -----------------------------------------------------------------------------
// I2C master configuration for temperature sensor(s)
// -----------------------------------------------------------------------------
#define I2C_MASTER_SCL_IO        22          // ESP32 GPIO for I2C SCL
#define I2C_MASTER_SDA_IO        21          // ESP32 GPIO for I2C SDA
#define I2C_MASTER_PORT          I2C_NUM_0   // I2C controller instance
#define I2C_MASTER_FREQ_HZ       100000      // 100 kHz standard mode

// -----------------------------------------------------------------------------
// AHT20 temperature and humidity sensor
// -----------------------------------------------------------------------------
#define AHT20_I2C_ADDRESS        0x38

// Initialization command (see AHT20 datasheet)
#define AHT20_CMD_INIT_BYTE1     0xBE
#define AHT20_CMD_INIT_BYTE2     0x08
#define AHT20_CMD_INIT_BYTE3     0x00

// Measurement command (trigger temp + humidity conversion)
#define AHT20_CMD_MEASURE_BYTE1  0xAC
#define AHT20_CMD_MEASURE_BYTE2  0x33
#define AHT20_CMD_MEASURE_BYTE3  0x00

// Typical measurement time is around 80 ms, we wait a bit more for safety
#define AHT20_MEASURE_DELAY_MS   100

// Display/UI
#define TASK_PRIO_DISPLAY 3
#define TASK_STACK_DISPLAY 4096



// -----------------------------------------------------------------------------
// Wi-Fi / Network
// -----------------------------------------------------------------------------
// Replace with your real SSID and password
#define WIFI_SSID           "DUPA_2_4_G"
#define WIFI_PASS           "6045270435"

// Number of reconnect attempts before giving up (for now just log)
#define WIFI_MAX_RETRY      5

// NET task
#define TASK_PRIO_NET       4
#define TASK_STACK_NET      4096

// ---------------------------------------------------------------------------
// Telemetry server configuration
// ---------------------------------------------------------------------------
#define TH_SERVER_HOST   "10.0.0.79"
#define TH_SERVER_PORT   "8000"
#define TH_SERVER_API_KEY "super-secret-token"





// ---------------------------------------------------------------------------
// Telemetry Device Info
// ---------------------------------------------------------------------------
#pragma once
#define DEVICE_SERIAL "SN-ESP32-THERMO-001"

// This is the raw API key we got ONCE from /api/devices/register/
// For now we hard-code it. Later this will come from secure storage.
#define DEVICE_API_KEY  "gapK_SjuuRC5CX3NLQ59RE1hNHaeEeqlknQUi-1gIPI"


// Optional future: versioned API path
#define TH_API_INGEST_PATH "/api/telemetry/ingest/"


#endif  // CONFIG_H
