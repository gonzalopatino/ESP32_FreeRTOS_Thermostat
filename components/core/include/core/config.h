/**
 * @file    config.h
 * @brief   Central system configuration for ThermostatRTOS.
 *
 * This file contains all compile-time configuration parameters for the
 * thermostat firmware including:
 *   - Application identity and versioning
 *   - Task priorities and stack sizes
 *   - Hardware pin assignments
 *   - Peripheral configurations
 *   - Network and telemetry settings
 *
 * @note    Modify this file to customize the firmware for different
 *          hardware configurations or deployment environments.
 *
 * @author  Gonzalo Patino
 * @company ThinkSense Labs
 * @date    2024-2025
 *
 * @copyright Copyright (c) 2024-2025 ThinkSense Labs. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ═══════════════════════════════════════════════════════════════════════════
 * APPLICATION IDENTITY
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Application name displayed in logs and telemetry */
#define APP_NAME        "ThermostatRTOS"

/** Firmware version (semantic versioning) */
#define APP_FW_VERSION  "0.1.0"

/* ═══════════════════════════════════════════════════════════════════════════
 * RTOS TASK CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @name Task Priorities
 * Higher number = higher priority (FreeRTOS convention)
 * @{
 */
#define TASK_PRIO_LOGGER      5     /**< Logger task priority */
#define TASK_PRIO_SENSORS     4     /**< Sensor acquisition priority */
#define TASK_PRIO_HEARTBEAT   3     /**< Heartbeat/LED priority */
#define TASK_PRIO_CONTROL     5     /**< Control loop priority */
/** @} */

/**
 * @name Task Stack Sizes
 * Stack sizes in words (ESP-IDF uses 4-byte words)
 * @{
 */
#define TASK_STACK_LOGGER     4096  /**< Logger task stack */
#define TASK_STACK_SENSORS    4096  /**< Sensor task stack */
#define TASK_STACK_HEARTBEAT  4096  /**< Heartbeat task stack */
#define TASK_STACK_CONTROL    4096  /**< Control task stack */
/** @} */

/* ═══════════════════════════════════════════════════════════════════════════
 * TIMING CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

#define PERIOD_LOGGER_MS      50    /**< Logger wake period (ms) */
#define PERIOD_SENSORS_MS     500   /**< Sensor sampling period (ms) */
#define TELEMETRY_PERIOD_MS   15000 /**< Cloud telemetry interval (ms) */

/* ═══════════════════════════════════════════════════════════════════════════
 * LOGGING SUBSYSTEM
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LOG_BUFFER_LEN        256   /**< Maximum log message length */
#define LOG_QUEUE_LENGTH      32    /**< Logging queue depth */

/* ═══════════════════════════════════════════════════════════════════════════
 * THERMOSTAT CONTROL PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define THERMOSTAT_SETPOINT_C       22.0f  /**< Default setpoint (°C) */
#define THERMOSTAT_HYSTERESIS_C      0.5f  /**< Hysteresis band (±°C) */

/* ═══════════════════════════════════════════════════════════════════════════
 * GPIO PIN ASSIGNMENTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @name System GPIOs
 * @{
 */
#define LED_GPIO              2     /**< Heartbeat LED */
#define GPIO_HEAT_OUTPUT      12    /**< Heater relay output */
#define GPIO_COOL_OUTPUT      33    /**< Cooler relay output */
/** @} */

/**
 * @name LCD Display (HD44780 4-bit mode)
 * @{
 */
#define LCD_PIN_RS           19     /**< Register select pin */
#define LCD_PIN_EN           18     /**< Enable pin */
#define LCD_PIN_D4           5      /**< Data bit 4 */
#define LCD_PIN_D5           17     /**< Data bit 5 */
#define LCD_PIN_D6           16     /**< Data bit 6 */
#define LCD_PIN_D7           4      /**< Data bit 7 */
/** @} */

/**
 * @name Button Inputs
 * @{
 */
#define GPIO_BTN_UP              25  /**< Setpoint increase button */
#define GPIO_BTN_DOWN            26  /**< Setpoint decrease button */
#define GPIO_BTN_MODE            27  /**< Mode cycle button */
#define BUTTON_EVENT_QUEUE_LEN   8   /**< Button event queue depth */
#define BUTTON_DEBOUNCE_MS       200 /**< Debounce time (ms) */
/** @} */

/**
 * @name Setpoint Limits
 * @{
 */
#define THERMOSTAT_SP_STEP_C     0.5f   /**< Adjustment step (°C) */
#define THERMOSTAT_SP_MIN_C      15.0f  /**< Minimum setpoint (°C) */
#define THERMOSTAT_SP_MAX_C      28.0f  /**< Maximum setpoint (°C) */
/** @} */

/**
 * @name Button Task Configuration
 * @{
 */
#define TASK_PRIO_BUTTONS        4      /**< Button task priority */
#define TASK_STACK_BUTTONS       4096   /**< Button task stack */
/** @} */

/* ═══════════════════════════════════════════════════════════════════════════
 * I2C CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

#define I2C_MASTER_SCL_IO        22          /**< I2C clock GPIO */
#define I2C_MASTER_SDA_IO        21          /**< I2C data GPIO */
#define I2C_MASTER_PORT          I2C_NUM_0   /**< I2C controller */
#define I2C_MASTER_FREQ_HZ       100000      /**< I2C frequency (Hz) */

/* ═══════════════════════════════════════════════════════════════════════════
 * AHT20 TEMPERATURE/HUMIDITY SENSOR
 * ═══════════════════════════════════════════════════════════════════════════ */

#define AHT20_I2C_ADDRESS        0x38        /**< AHT20 I2C address */

/** @name AHT20 Commands */
#define AHT20_CMD_INIT_BYTE1     0xBE
#define AHT20_CMD_INIT_BYTE2     0x08
#define AHT20_CMD_INIT_BYTE3     0x00
#define AHT20_CMD_MEASURE_BYTE1  0xAC
#define AHT20_CMD_MEASURE_BYTE2  0x33
#define AHT20_CMD_MEASURE_BYTE3  0x00
#define AHT20_MEASURE_DELAY_MS   100         /**< Measurement wait (ms) */
/** @} */

/**
 * @name Display Task Configuration
 * @{
 */
#define TASK_PRIO_DISPLAY        3           /**< Display task priority */
#define TASK_STACK_DISPLAY       4096        /**< Display task stack */
/** @} */

/* ═══════════════════════════════════════════════════════════════════════════
 * WIFI / NETWORK CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @name WiFi Credentials (Development Only)
 * @warning These are overwritten by provisioning in production
 * @{
 */
#define WIFI_SSID           "DUPA_2_4_G"
#define WIFI_PASS           "6045270435"
#define WIFI_MAX_RETRY      5                /**< Connection retry count */
/** @} */

/**
 * @name Network Task Configuration
 * @{
 */
#define TASK_PRIO_NET       4                /**< Network task priority */
#define TASK_STACK_NET      4096             /**< Network task stack */
/** @} */

/* ═══════════════════════════════════════════════════════════════════════════
 * TELEMETRY SERVER CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TH_SERVER_HOST      "10.0.0.79"      /**< Backend server IP */
#define TH_SERVER_PORT      "8000"           /**< Backend server port */
#define TH_API_INGEST_PATH  "/api/telemetry/ingest/"

/* ═══════════════════════════════════════════════════════════════════════════
 * DEVICE IDENTITY
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @name Device Defaults
 * Used if NVS does not have values; overwritten by provisioning
 * @{
 */
#define DEVICE_SERIAL           "SN-ESP32-THERO-004"
#define DEVICE_SERIAL_DEFAULT   "SN-ESP32-THERO-004"
#define DEVICE_API_KEY          "45BVjokdV1YehGxDaPNpb7eT2CBPdWRiAoqpkfsLJxs"
#define DEVICE_API_KEY_DEFAULT  "45BVjokdV1YehGxDaPNpb7eT2CBPdWRiAoqpkfsLJxs"
/** @} */

#endif  /* CONFIG_H */
