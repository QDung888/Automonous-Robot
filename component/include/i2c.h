/**
 * @file i2c.h
 * @brief I2C Master Bus – khởi tạo và quản lý bus I2C dùng chung
 */

#ifndef I2C_H
#define I2C_H

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Cấu hình chân I2C mặc định ---- */
#define I2C_DEFAULT_PORT    I2C_NUM_0
#define I2C_SCL_IO          GPIO_NUM_40
#define I2C_SDA_IO          GPIO_NUM_41

/**
 * @brief Khởi tạo I2C master bus với cấu hình mặc định
 *
 * @param[out] bus_handle  Con trỏ nhận handle bus I2C
 * @return esp_err_t       ESP_OK nếu thành công
 */
esp_err_t i2c_master_bus_init(i2c_master_bus_handle_t *bus_handle);

/**
 * @brief Lấy handle bus I2C đã khởi tạo (singleton)
 *
 * @return i2c_master_bus_handle_t  Handle bus hoặc NULL nếu chưa khởi tạo
 */
i2c_master_bus_handle_t i2c_master_bus_get(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
