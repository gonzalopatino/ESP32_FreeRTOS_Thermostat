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
#define GPIO_HEAT_OUTPUT      4   // Heater relay / output pin

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

#endif  // CONFIG_H
