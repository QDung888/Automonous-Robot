/**
 * @file serial_ros.h
 * @brief Truyền nhận dữ liệu encoder qua UART0 (CP2102) với Raspberry Pi
 *
 * Tham khảo: rosserial_esp32 (sachin0x18)
 *   - UART driver trên ESP-IDF
 *   - Task pinned to Core 1 để đảm bảo ổn định
 *
 * Giao thức text-based:
 *   ESP32 → Pi:  $ENC,<tick_AL>,<tick_BL>,<tick_AR>,<tick_BR>\r\n
 *   Pi → ESP32:  $CMD,<dir>,<speed>\r\n
 *                dir: F=forward, B=backward, L=left, R=right, S=stop
 *                speed: 0-255
 *
 * UART0: TX0=GPIO43, RX0=GPIO44 (kết nối CP2102 → Raspberry Pi)
 * Tất cả task chạy trên Core 1.
 */

#ifndef SERIAL_ROS_H
#define SERIAL_ROS_H

#include <stdint.h>
#include "esp_err.h"
#include "TB6612FNG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Cấu hình ==================== */

#define SERIAL_ROS_UART_NUM         0           /* UART0 – qua CP2102 */
#define SERIAL_ROS_BAUD_RATE        115200
#define SERIAL_ROS_TX_IO            43          /* TX0 */
#define SERIAL_ROS_RX_IO            44          /* RX0 */
#define SERIAL_ROS_BUF_SIZE         1024        /* RX/TX buffer (theo rosserial) */
#define SERIAL_ROS_TX_INTERVAL_MS   50          /* Gửi encoder data mỗi 50ms (20Hz) */
#define SERIAL_ROS_CORE             1           /* Chạy trên Core 1 */
#define SERIAL_ROS_TX_PRIORITY      7           /* Priority cao cho TX task */
#define SERIAL_ROS_RX_PRIORITY      6           /* Priority cho RX task */

/* ==================== Callback cho lệnh nhận từ Pi ==================== */

/**
 * @brief Callback khi nhận được lệnh motor từ Raspberry Pi
 *
 * @param direction  Hướng: CAR_FORWARD, CAR_BACKWARD, CAR_TURN_LEFT, CAR_TURN_RIGHT, CAR_STOP
 * @param speed      Tốc độ PWM (0-255)
 */
typedef void (*serial_ros_cmd_cb_t)(uint8_t direction, uint8_t speed);

/* ==================== API ==================== */

/**
 * @brief Khởi tạo UART0 cho SerialROS
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t serial_ros_init(void);

/**
 * @brief Bắt đầu task gửi encoder data + nhận lệnh từ Pi (pinned to Core 1)
 *
 * @param motor_sys  Con trỏ tới TB6612 system (để đọc encoder)
 * @param cmd_cb     Callback xử lý lệnh motor từ Pi (có thể NULL)
 */
void serial_ros_start(tb6612_system_t *motor_sys, serial_ros_cmd_cb_t cmd_cb);

/**
 * @brief Gửi 1 gói encoder data thủ công (nếu không dùng task)
 *
 * @param ticks  Mảng 4 giá trị tick [AL, BL, AR, BR]
 */
void serial_ros_send_encoder(const int ticks[4]);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_ROS_H */
