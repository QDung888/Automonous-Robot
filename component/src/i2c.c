/**
 * @file i2c.c
 * @brief I2C Master Bus – implementation
 */

#include "i2c.h"
#include "esp_log.h"

static const char *TAG = "I2C";

/* Singleton bus handle */
static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t i2c_master_bus_init(i2c_master_bus_handle_t *bus_handle)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Nếu đã khởi tạo rồi thì trả về handle cũ */
    if (s_bus_handle != NULL) {
        *bus_handle = s_bus_handle;
        ESP_LOGI(TAG, "I2C bus đã được khởi tạo trước đó");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_DEFAULT_PORT,
        .scl_io_num = I2C_SCL_IO,
        .sda_io_num = I2C_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = false },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Tạo I2C bus thất bại: %s", esp_err_to_name(err));
        s_bus_handle = NULL;
        return err;
    }

    *bus_handle = s_bus_handle;
    ESP_LOGI(TAG, "I2C bus OK (SCL=IO%d, SDA=IO%d)", I2C_SCL_IO, I2C_SDA_IO);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_master_bus_get(void)
{
    return s_bus_handle;
}
