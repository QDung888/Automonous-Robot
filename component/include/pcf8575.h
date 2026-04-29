/**
 * @file pcf8575.h
 * @brief PCF8575 16-bit I2C GPIO Expander Driver
 *
 * API đơn giản để ghi/đọc 16 chân I/O qua I2C.
 * Địa chỉ mặc định: 0x20 (A0=A1=A2=GND)
 */

#ifndef PCF8575_H
#define PCF8575_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCF8575_DEFAULT_ADDR    0x20

/**
 * @brief Handle cho PCF8575
 */
typedef struct {
    i2c_master_bus_handle_t bus_handle;  /*!< Handle bus I2C */
    i2c_master_dev_handle_t dev_handle; /*!< Handle device I2C */
    uint8_t i2c_address;                /*!< Địa chỉ I2C */
    uint16_t output_state;              /*!< Trạng thái output hiện tại */
    bool initialized;                   /*!< Cờ đã khởi tạo */
} pcf8575_handle_t;

/** Macro khởi tạo mặc định */
#define PCF8575_HANDLE_DEFAULT() { \
    .bus_handle = NULL,             \
    .dev_handle = NULL,             \
    .i2c_address = PCF8575_DEFAULT_ADDR, \
    .output_state = 0x0000,         \
    .initialized = false            \
}

/**
 * @brief Khởi tạo PCF8575 trên bus I2C
 *
 * @param handle     Con trỏ tới handle PCF8575 (cần set i2c_address trước)
 * @param bus_handle Handle bus I2C master đã khởi tạo
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t pcf8575_init(pcf8575_handle_t *handle, i2c_master_bus_handle_t bus_handle);

/**
 * @brief Ghi toàn bộ 16 bit output
 *
 * @param handle Con trỏ tới handle PCF8575
 * @param value  Giá trị 16 bit (P0-P7 = byte thấp, P8-P15 = byte cao)
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t pcf8575_write(pcf8575_handle_t *handle, uint16_t value);

/**
 * @brief Đọc toàn bộ 16 bit
 *
 * @param handle Con trỏ tới handle PCF8575
 * @param value  Con trỏ nhận giá trị 16 bit
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t pcf8575_read(pcf8575_handle_t *handle, uint16_t *value);

/**
 * @brief Ghi giá trị vào 1 pin (0-15)
 */
esp_err_t pcf8575_write_pin(pcf8575_handle_t *handle, uint8_t pin, bool level);

/**
 * @brief Ghi nhiều pin cùng lúc bằng mask
 *
 * @param handle      Con trỏ tới handle
 * @param pins_mask   Các pin cần thay đổi
 * @param values_mask Giá trị tương ứng
 */
esp_err_t pcf8575_write_pins(pcf8575_handle_t *handle, uint16_t pins_mask, uint16_t values_mask);

#ifdef __cplusplus
}
#endif

#endif /* PCF8575_H */
