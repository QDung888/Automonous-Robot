/**
 * @file joystick.h
 * @brief Driver tay cầm PS2 wireless cho điều khiển xe robot
 *
 * Giao tiếp qua SPI bit-bang (spi.h).
 * Hỗ trợ D-pad và analog stick để điều khiển tiến/lùi/trái/phải.
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "TB6612FNG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== PS2 Button Masks ==================== */

/*
 * Byte 3 – Digital buttons 1 (active LOW → bit=0 khi nhấn)
 */
#define PS2_BTN_SELECT      (1 << 0)
#define PS2_BTN_L3          (1 << 1)    /* Left stick press */
#define PS2_BTN_R3          (1 << 2)    /* Right stick press */
#define PS2_BTN_START       (1 << 3)
#define PS2_BTN_UP          (1 << 4)
#define PS2_BTN_RIGHT       (1 << 5)
#define PS2_BTN_DOWN        (1 << 6)
#define PS2_BTN_LEFT        (1 << 7)

/*
 * Byte 4 – Digital buttons 2 (active LOW)
 */
#define PS2_BTN_L2          (1 << 0)
#define PS2_BTN_R2          (1 << 1)
#define PS2_BTN_L1          (1 << 2)
#define PS2_BTN_R1          (1 << 3)
#define PS2_BTN_TRIANGLE    (1 << 4)
#define PS2_BTN_CIRCLE      (1 << 5)
#define PS2_BTN_CROSS       (1 << 6)
#define PS2_BTN_SQUARE      (1 << 7)

/* ==================== Kiểu dữ liệu ==================== */

/**
 * @brief Dữ liệu trạng thái tay cầm PS2
 */
typedef struct {
    uint8_t mode;           /*!< 0x41=digital, 0x73=analog, 0x79=analog+pressure */
    uint8_t buttons1;       /*!< Digital buttons byte 1 (active LOW) */
    uint8_t buttons2;       /*!< Digital buttons byte 2 (active LOW) */
    uint8_t right_x;        /*!< Right stick X (0-255, center ~128) */
    uint8_t right_y;        /*!< Right stick Y (0-255, center ~128) */
    uint8_t left_x;         /*!< Left stick X (0-255, center ~128) */
    uint8_t left_y;         /*!< Left stick Y (0-255, center ~128) */
    bool connected;         /*!< Tay cầm có kết nối hay không */
} ps2_data_t;

/* ==================== API ==================== */

/**
 * @brief Khởi tạo tay cầm PS2 (init SPI + enter analog mode)
 *
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t joystick_init(void);

/**
 * @brief Đọc trạng thái tay cầm PS2
 *
 * @param data Con trỏ nhận dữ liệu
 * @return esp_err_t ESP_OK nếu đọc thành công
 */
esp_err_t joystick_poll(ps2_data_t *data);

/**
 * @brief Kiểm tra nút có được nhấn không (byte 1)
 *
 * @param data   Dữ liệu PS2
 * @param button Mask nút (PS2_BTN_UP, PS2_BTN_DOWN, ...)
 * @return true  Nút đang được nhấn
 */
static inline bool joystick_btn1_pressed(const ps2_data_t *data, uint8_t button)
{
    /* Active LOW: bit=0 → nhấn */
    return !(data->buttons1 & button);
}

/**
 * @brief Kiểm tra nút có được nhấn không (byte 2)
 */
static inline bool joystick_btn2_pressed(const ps2_data_t *data, uint8_t button)
{
    return !(data->buttons2 & button);
}

/**
 * @brief Tạo task điều khiển xe bằng tay cầm PS2
 *
 * Task sẽ poll tay cầm liên tục và điều khiển motor qua TB6612.
 *
 * @param motor_sys Con trỏ tới TB6612 system handle
 */
void joystick_start_control_task(tb6612_system_t *motor_sys);

#ifdef __cplusplus
}
#endif

#endif /* JOYSTICK_H */
