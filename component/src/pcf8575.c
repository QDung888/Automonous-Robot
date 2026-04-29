/**
 * @file pcf8575.c
 * @brief PCF8575 16-bit I2C GPIO Expander Driver – Implementation
 */

#include "pcf8575.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PCF8575";

#define PCF8575_RETRY_COUNT     3
#define PCF8575_RETRY_DELAY_MS  20

esp_err_t pcf8575_init(pcf8575_handle_t *handle, i2c_master_bus_handle_t bus_handle)
{
    if (handle == NULL || bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->bus_handle = bus_handle;

    /* Thêm device vào bus I2C – Fast Mode 400kHz */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = handle->i2c_address,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &handle->dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể thêm PCF8575 (0x%02X) vào bus: %s",
                 handle->i2c_address, esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    /* Ghi trạng thái ban đầu (tất cả LOW) */
    handle->output_state = 0x0000;
    uint8_t data[2] = { 0x00, 0x00 };
    err = i2c_master_transmit(handle->dev_handle, data, 2, 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ghi trạng thái ban đầu thất bại: %s", esp_err_to_name(err));
        return err;
    }

    handle->initialized = true;
    ESP_LOGI(TAG, "PCF8575 OK (addr=0x%02X)", handle->i2c_address);
    return ESP_OK;
}

esp_err_t pcf8575_write(pcf8575_handle_t *handle, uint16_t value)
{
    if (handle == NULL || handle->dev_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * PCF8575 nhận 2 byte:
     *   Byte 0: P0-P7  (low byte)
     *   Byte 1: P8-P15 (high byte)
     */
    uint8_t data[2] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
    };

    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < PCF8575_RETRY_COUNT; retry++) {
        err = i2c_master_transmit(handle->dev_handle, data, 2, 1000);
        if (err == ESP_OK) {
            break;
        }
        if (retry < PCF8575_RETRY_COUNT - 1) {
            vTaskDelay(pdMS_TO_TICKS(PCF8575_RETRY_DELAY_MS));
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ghi PCF8575 thất bại: 0x%04X – %s", value, esp_err_to_name(err));
        return err;
    }

    handle->output_state = value;
    return ESP_OK;
}

esp_err_t pcf8575_read(pcf8575_handle_t *handle, uint16_t *value)
{
    if (handle == NULL || !handle->initialized || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {0};
    esp_err_t err = i2c_master_receive(handle->dev_handle, data, 2, 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Đọc PCF8575 thất bại: %s", esp_err_to_name(err));
        return err;
    }

    *value = ((uint16_t)data[1] << 8) | data[0];
    return ESP_OK;
}

esp_err_t pcf8575_write_pin(pcf8575_handle_t *handle, uint8_t pin, bool level)
{
    if (pin > 15) {
        return ESP_ERR_INVALID_ARG;
    }
    if (level) {
        handle->output_state |= (1 << pin);
    } else {
        handle->output_state &= ~(1 << pin);
    }
    return pcf8575_write(handle, handle->output_state);
}

esp_err_t pcf8575_write_pins(pcf8575_handle_t *handle, uint16_t pins_mask, uint16_t values_mask)
{
    if (handle == NULL) return ESP_ERR_INVALID_ARG;

    uint16_t new_state = handle->output_state;
    new_state &= ~pins_mask;
    new_state |= (values_mask & pins_mask);

    return pcf8575_write(handle, new_state);
}
