#ifndef VL53L1X_H
#define VL53L1X_H

/**
 * @file vl53l1x.h
 * @brief Driver VL53L1X – hỗ trợ 2 cảm biến trên cùng bus I2C
 *
 * Sơ đồ kết nối:
 *   SDA  → IO41 (chung I2C bus)
 *   SCL  → IO40 (chung I2C bus)
 *
 *   Sensor 1:  XSHUT → IO36,  GPIO1 → IO35
 *   Sensor 2:  XSHUT → IO39,  GPIO1 → IO38
 *
 * Khởi tạo:
 *   1. Cả 2 XSHUT LOW (reset)
 *   2. XSHUT1 HIGH → sensor1 boot ở 0x29 → đổi thành 0x30
 *   3. XSHUT2 HIGH → sensor2 boot ở 0x29 (giữ nguyên)
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Cấu hình chân ==================== */

/* Sensor 1 */
#define VL53L1X_XSHUT_1_GPIO    GPIO_NUM_36
#define VL53L1X_GPIO1_1_GPIO    GPIO_NUM_35

/* Sensor 2 */
#define VL53L1X_XSHUT_2_GPIO    GPIO_NUM_39
#define VL53L1X_GPIO1_2_GPIO    GPIO_NUM_38

/* I2C address */
#define VL53L1X_DEFAULT_ADDR    0x29    /* Địa chỉ mặc định (7-bit) */
#define VL53L1X_SENSOR1_ADDR    0x30    /* Sensor 1 – đổi sau khi boot */
#define VL53L1X_SENSOR2_ADDR    0x29    /* Sensor 2 – giữ mặc định */

#define VL53L1X_SENSOR_COUNT    2

/* ==================== Kiểu dữ liệu ==================== */

typedef enum {
    VL53L1X_SENSOR_1 = 0,
    VL53L1X_SENSOR_2 = 1,
} vl53l1x_sensor_id_t;

typedef enum {
    VL53L1X_DIST_SHORT = 1,     /* < 1.3m */
    VL53L1X_DIST_LONG  = 2,     /* < 4m */
} vl53l1x_dist_mode_t;

/* ==================== API ==================== */

/**
 * @brief Khởi tạo 2 cảm biến VL53L1X trên cùng bus I2C
 *
 * @param bus_handle  I2C bus handle đã khởi tạo
 * @return esp_err_t  ESP_OK nếu thành công
 */
esp_err_t vl53l1x_dual_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Đọc khoảng cách từ 1 sensor (mm)
 *
 * @param sensor_id  VL53L1X_SENSOR_1 hoặc VL53L1X_SENSOR_2
 * @return uint16_t  Khoảng cách (mm), 0 nếu lỗi
 */
uint16_t vl53l1x_read_distance(vl53l1x_sensor_id_t sensor_id);

/**
 * @brief Tạo task đọc + in khoảng cách 2 sensor ra monitor (mỗi 200ms)
 */
void vl53l1x_start_print_task(void);

#ifdef __cplusplus
}
#endif

#endif /* VL53L1X_H */
