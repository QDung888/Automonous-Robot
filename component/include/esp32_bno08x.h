#ifndef ESP32_BNO08X_H
#define ESP32_BNO08X_H

#include <stdint.h>
#include <stdbool.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP32_BNO08X_RESET_REASON_POR = 0,
    ESP32_BNO08X_RESET_REASON_INT_RST,
    ESP32_BNO08X_RESET_REASON_WTD,
    ESP32_BNO08X_RESET_REASON_EXT_RST,
    ESP32_BNO08X_RESET_REASON_OTHER,
    ESP32_BNO08X_RESET_REASON_UNDEFINED
} esp32_bno08x_reset_reason_t;

typedef struct {
    spi_host_device_t spi_peripheral;
    gpio_num_t io_mosi;
    gpio_num_t io_miso;
    gpio_num_t io_sclk;
    gpio_num_t io_cs;
    gpio_num_t io_int;
    gpio_num_t io_rst;
    uint32_t sclk_speed;
    bool install_isr_service;
} esp32_bno08x_config_t;

typedef struct esp32_bno08x_handle_t esp32_bno08x_handle_t;

esp32_bno08x_handle_t* esp32_bno08x_create(const esp32_bno08x_config_t* config);
void esp32_bno08x_destroy(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_initialize(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_hard_reset(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_soft_reset(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_disable_all_reports(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_on(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_sleep(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_data_available(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_has_game_rotation_vector(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_enable_game_rotation_vector(esp32_bno08x_handle_t* handle, uint32_t period_us);
bool esp32_bno08x_get_game_rotation_vector(esp32_bno08x_handle_t* handle, float* roll, float* pitch, float* yaw);

bool esp32_bno08x_has_calibrated_gyro(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_enable_calibrated_gyro(esp32_bno08x_handle_t* handle, uint32_t period_us);
bool esp32_bno08x_get_calibrated_gyro(esp32_bno08x_handle_t* handle, float* x, float* y, float* z);
 
bool esp32_bno08x_has_linear_acceleration(esp32_bno08x_handle_t* handle);
bool esp32_bno08x_enable_linear_acceleration(esp32_bno08x_handle_t* handle, uint32_t period_us);
bool esp32_bno08x_get_linear_acceleration(esp32_bno08x_handle_t* handle, float* x, float* y, float* z);
 
bool esp32_bno08x_get_system_orientation(esp32_bno08x_handle_t* handle, float* w, float* x, float* y, float* z);
bool esp32_bno08x_register_callback(esp32_bno08x_handle_t* handle, void (*cb)(void));
esp32_bno08x_reset_reason_t esp32_bno08x_get_reset_reason(esp32_bno08x_handle_t* handle);

#ifdef __cplusplus
}
#endif

#endif // ESP32_BNO08X_H