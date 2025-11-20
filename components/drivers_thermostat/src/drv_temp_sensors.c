/**
 * @file drv_temp_sensors.c
 * @brief Temperature sensor driver for the thermostat firmware.
 *
 * This module provides a hardware abstraction around the temperature sensors
 * used by the thermostat. It currently implements support for a single AHT20
 * digital temperature (and humidity) sensor connected over I2C.
 *
 * Responsibilities:
 *   - Initialize the I2C bus and AHT20 device
 *   - Trigger measurements on the AHT20
 *   - Convert raw measurement bytes into Celsius
 *   - Populate sensor_sample_t used by the SENSORS task
 *
 * Future extensions:
 *   - Add a second physical sensor for true indoor/outdoor readings
 *   - Add humidity reporting
 *   - Error counters and self-diagnostics
 */

#include "drivers/drv_temp_sensors.h"
#include "core/logging.h"
#include "core/config.h"
#include "core/error.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"   // For GPIO_PULLUP_ENABLE, pin types

static const char *TAG = "DRV_TS";

// Tracks whether the AHT20 has been successfully initialized.
static bool g_aht20_initialized = false;


/**
 * @brief Configure and initialize the I2C master peripheral.
 *
 * Uses parameters defined in config.h (I2C_MASTER_PORT, I2C_MASTER_SDA_IO,
 * I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ).
 *
 * @return ESP_OK on success, ESP_FAIL / ESP_ERR_* on error.
 */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ   // Only clk_speed here in this IDF version
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (err != ESP_OK) {
        return err;
    }

     // Install I2C driver with no RX/TX buffers for master mode
    err = i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    

    return err;
}


/**
 * @brief Write a sequence of bytes to the AHT20 over I2C.
 *
 * @param[in] data Pointer to bytes to send.
 * @param[in] len  Number of bytes to write.
 *
 * @return ESP_OK on success, ESP_ERR_* on I2C errors.
 */
static esp_err_t aht20_write_cmd(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_FAIL;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,
                          (AHT20_I2C_ADDRESS << 1) | I2C_MASTER_WRITE,
                          true /* expect ACK */);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(
        I2C_MASTER_PORT,
        cmd,
        pdMS_TO_TICKS(100)   // Command timeout
    );

    i2c_cmd_link_delete(cmd);
    return err;
}


/**
 * @brief Read a sequence of bytes from the AHT20 over I2C.
 *
 * @param[out] data Buffer to store received bytes.
 * @param[in]  len  Number of bytes to read.
 *
 * @return ESP_OK on success, ESP_ERR_* on I2C errors.
 */
static esp_err_t aht20_read_bytes(uint8_t *data, size_t len)
{
    if (len == 0 || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_FAIL;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,
                          (AHT20_I2C_ADDRESS << 1) | I2C_MASTER_READ,
                          true /* expect ACK */);

    // Read all but the last byte with ACK, and the last byte with NACK.
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(
        I2C_MASTER_PORT,
        cmd,
        pdMS_TO_TICKS(100)
    );

    i2c_cmd_link_delete(cmd);
    return err;
}


/**
 * @brief Send AHT20 initialization command sequence.
 *
 * The AHT20 requires an initialization sequence after power-up.
 * Command bytes are defined in config.h for flexibility.
 *
 * @return ESP_OK on success, ESP_ERR_* on I2C errors.
 */
static esp_err_t aht20_init(void)
{
    // Initialization command (e.g., 0xBE 0x08 0x00 depending on macros)
    uint8_t init_cmd[3] = {
        AHT20_CMD_INIT_BYTE1,
        AHT20_CMD_INIT_BYTE2,
        AHT20_CMD_INIT_BYTE3
    };

    esp_err_t err = aht20_write_cmd(init_cmd, sizeof(init_cmd));
    if (err != ESP_OK) {
        return err;
    }

    // Datasheet recommends a short delay after init.
    vTaskDelay(pdMS_TO_TICKS(40));

    return ESP_OK;
}


/**
 * @brief Trigger a measurement on the AHT20 and read raw bytes.
 *
 * This function:
 *   - Sends the measurement command
 *   - Waits for the conversion time
 *   - Reads 6 bytes from the sensor (status + 5 data bytes)
 *
 * @param[out] buf Buffer to hold at least 6 bytes of raw data.
 * @param[in]  len Buffer length in bytes (must be >= 6).
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_SIZE or ESP_ERR_* on error.
 */
static esp_err_t aht20_measure_raw(uint8_t *buf, size_t len)
{
    if (buf == NULL || len < 6) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Measurement command (e.g. 0xAC 0x33 0x00 via macros).
    uint8_t cmd[3] = {
        AHT20_CMD_MEASURE_BYTE1,
        AHT20_CMD_MEASURE_BYTE2,
        AHT20_CMD_MEASURE_BYTE3
    };

    // Send measurement command
    esp_err_t err = aht20_write_cmd(cmd, sizeof(cmd));
    if (err != ESP_OK) {
        return err;
    }

    // Wait for sensor to complete the measurement.
    vTaskDelay(pdMS_TO_TICKS(AHT20_MEASURE_DELAY_MS));

    // Read 6 bytes: [0] status, [1..5] humidity + temperature bits
    err = aht20_read_bytes(buf, 6);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}


/**
 * @brief Convert raw AHT20 bytes into temperature in Celsius.
 *
 * The AHT20 encodes temperature as a 20-bit value spread across
 * bytes buf[3], buf[4], buf[5]:
 *
 *   adc_T = ((buf[3] & 0x0F) << 16) | (buf[4] << 8) | buf[5]
 *
 * According to the datasheet:
 *
 *   T(°C) = (adc_T / 2^20) * 200 - 50
 *
 * @param[in]  buf        Raw 6-byte measurement buffer from AHT20.
 * @param[out] out_temp_c Pointer to float where the result will be stored.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on null pointers.
 */
static esp_err_t aht20_raw_to_temp_c(const uint8_t *buf, float *out_temp_c)
{
    if (buf == NULL || out_temp_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t adc_T =
        ((uint32_t)(buf[3] & 0x0F) << 16) |
        ((uint32_t)buf[4] << 8) |
        (uint32_t)buf[5];

    float t = ((float)adc_T * 200.0f / 1048576.0f) - 50.0f;  // 2^20 = 1048576
    *out_temp_c = t;

    return ESP_OK;
}


/**
 * @brief Initialize the temperature sensor subsystem.
 *
 * This function:
 *   - Configures the I2C master peripheral
 *   - Initializes the AHT20 sensor
 *   - Marks the driver as ready for use
 *
 * Called once from the SENSORS task during startup.
 *
 * @return APP_ERR_OK on success, ERR_GENERIC on failure.
 */
app_error_t drv_temp_sensors_init(void)
{
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "I2C init failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    err = aht20_init();
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "AHT20 init failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    g_aht20_initialized = true;

    log_post(LOG_LEVEL_INFO, TAG,
             "Temperature sensors initialized, AHT20 ready");

    return APP_ERR_OK;
}


/**
 * @brief Read indoor and outdoor temperatures from hardware.
 *
 * Current implementation:
 *   - Reads indoor temperature from AHT20 (Tin)
 *   - Sets outdoor temperature (Tout) equal to Tin as a placeholder
 *
 * In the future, this function can:
 *   - Use a second sensor for Tout
 *   - Fuse multiple sensors or apply filtering
 *
 * @param[out] out_sample Pointer to caller-allocated sensor_sample_t.
 *
 * @return APP_ERR_OK on success, ERR_GENERIC on invalid args or hardware errors.
 */
app_error_t drv_temp_read(sensor_sample_t *out_sample)
{
    if (out_sample == NULL) {
        return ERR_GENERIC;
    }

    if (!g_aht20_initialized) {
        // Driver was not initialized; this indicates a programming or startup order bug.
        log_post(LOG_LEVEL_ERROR, TAG,
                 "drv_temp_read called before drv_temp_sensors_init");
        return ERR_GENERIC;
    }

    uint8_t buf[6] = {0};

    esp_err_t err = aht20_measure_raw(buf, sizeof(buf));
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "AHT20 measure failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    float tin_c = 0.0f;
    err = aht20_raw_to_temp_c(buf, &tin_c);
    if (err != ESP_OK) {
        log_post(LOG_LEVEL_ERROR, TAG,
                 "AHT20 temperature conversion failed, err=%d", (int)err);
        return ERR_GENERIC;
    }

    // Populate the shared sample structure used by the rest of the system.
    out_sample->temp_inside_c  = tin_c;
    out_sample->temp_outside_c = tin_c;  // Placeholder: single physical sensor for now

    out_sample->timestamp_ms =
        (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    return APP_ERR_OK;
}
