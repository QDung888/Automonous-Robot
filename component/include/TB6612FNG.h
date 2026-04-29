/**
 * @file TB6612FNG.h
 * @brief Driver điều khiển 2x TB6612FNG cho xe 4 bánh
 *
 * Sơ đồ kết nối – 2 con TB6612 điều khiển qua PCF8575 (0x20):
 *
 * TB6612_RIGHT (P0..P3):
 *   P0 = BIN1_R        STBY_R  → IO48
 *   P1 = BIN2_R        PWMA_R  → IO9   (Motor A right)
 *   P2 = AIN1_R        PWMB_R  → IO47  (Motor B right)
 *   P3 = AIN2_R
 *
 * TB6612_LEFT (P4..P7):
 *   P4 = BIN1_L        STBY_L  → IO45
 *   P5 = BIN2_L        PWMA_L  → IO8   (Motor A left)
 *   P6 = AIN1_L        PWMB_L  → IO37  (Motor B left)
 *   P7 = AIN2_L
 *
 * Encoder (quadrature):
 *   LEFT:  C1L=IO4, C2L=IO5  (Motor A left)
 *          C3L=IO6, C4L=IO7  (Motor B left)
 *   RIGHT: C1R=IO15, C2R=IO16 (Motor A right)
 *          C3R=IO17, C4R=IO18 (Motor B right)
 */

#ifndef TB6612FNG_H
#define TB6612FNG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "pcf8575.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Cấu hình chân mặc định ==================== */

/* TB6612_RIGHT */
#define TB6612_STBY_R_GPIO      GPIO_NUM_48
#define TB6612_PWMA_R_GPIO      GPIO_NUM_9
#define TB6612_PWMB_R_GPIO      GPIO_NUM_47

/* TB6612_LEFT */
#define TB6612_STBY_L_GPIO      GPIO_NUM_45
#define TB6612_PWMA_L_GPIO      GPIO_NUM_8
#define TB6612_PWMB_L_GPIO      GPIO_NUM_37

/* Encoder pins – LEFT */
#define TB6612_ENC_AL_A         GPIO_NUM_4
#define TB6612_ENC_AL_B         GPIO_NUM_5
#define TB6612_ENC_BL_A         GPIO_NUM_6
#define TB6612_ENC_BL_B         GPIO_NUM_7

/* Encoder pins – RIGHT */
#define TB6612_ENC_AR_A         GPIO_NUM_15
#define TB6612_ENC_AR_B         GPIO_NUM_16
#define TB6612_ENC_BR_A         GPIO_NUM_17
#define TB6612_ENC_BR_B         GPIO_NUM_18

/* PWM */
#define TB6612_PWM_FREQ_HZ      1000
#define TB6612_PWM_RESOLUTION   8       /* 8-bit: 0-255 */
#define TB6612_PWM_MAX_DUTY     255

/* PCNT encoder limits */
#define TB6612_PCNT_HIGH_LIMIT  30000
#define TB6612_PCNT_LOW_LIMIT   (-30000)

/* ==================== PCF8575 bit mapping ==================== */

/*
 * RIGHT (P3..P0):
 *   Thuận:  P3=1,P2=0,P1=1,P0=0 → 0x0A
 *   Nghịch: P3=0,P2=1,P1=0,P0=1 → 0x05
 *
 * LEFT (P7..P4):
 *   Thuận:  P7=1,P6=0,P5=1,P4=0 → 0xA0
 *   Nghịch: P7=0,P6=1,P5=0,P4=1 → 0x50
 */
#define TB6612_RIGHT_FORWARD    0x0A
#define TB6612_RIGHT_REVERSE    0x05
#define TB6612_LEFT_FORWARD     0xA0
#define TB6612_LEFT_REVERSE     0x50
#define TB6612_ALL_STOP         0x00

/* ==================== Hướng di chuyển xe ==================== */

/**
 * Xe 4 bánh:
 *   Tiến:    LEFT nghịch + RIGHT thuận
 *   Lùi:     LEFT thuận  + RIGHT nghịch
 *   Rẽ trái: LEFT thuận  + RIGHT thuận
 *   Rẽ phải: LEFT nghịch + RIGHT nghịch
 */
#define CAR_FORWARD     (TB6612_LEFT_REVERSE  | TB6612_RIGHT_FORWARD)  /* 0x5A */
#define CAR_BACKWARD    (TB6612_LEFT_FORWARD  | TB6612_RIGHT_REVERSE)  /* 0xA5 */
#define CAR_TURN_LEFT   (TB6612_LEFT_FORWARD  | TB6612_RIGHT_FORWARD)  /* 0xAA */
#define CAR_TURN_RIGHT  (TB6612_LEFT_REVERSE  | TB6612_RIGHT_REVERSE)  /* 0x55 */
#define CAR_STOP        TB6612_ALL_STOP                                 /* 0x00 */

/* ==================== Kiểu dữ liệu ==================== */

/** ID encoder (4 motor) */
typedef enum {
    ENCODER_MOTOR_AL = 0,   /*!< Motor A left  */
    ENCODER_MOTOR_BL,       /*!< Motor B left  */
    ENCODER_MOTOR_AR,       /*!< Motor A right */
    ENCODER_MOTOR_BR,       /*!< Motor B right */
    ENCODER_COUNT           /*!< Tổng số encoder */
} encoder_id_t;

/**
 * @brief Handle cho toàn bộ hệ thống TB6612 (2 con + encoder)
 */
typedef struct {
    pcf8575_handle_t *pcf_handle;                    /*!< Handle PCF8575 dùng chung */
    pcnt_unit_handle_t enc_units[ENCODER_COUNT];     /*!< PCNT handles cho 4 encoder */
    bool initialized;                                /*!< Cờ đã khởi tạo */
} tb6612_system_t;

/* ==================== API ==================== */

/**
 * @brief Khởi tạo toàn bộ hệ thống TB6612 (STBY, PWM, encoder)
 *
 * @param sys        Con trỏ tới system handle
 * @param pcf_handle Con trỏ tới PCF8575 handle đã khởi tạo
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t tb6612_init(tb6612_system_t *sys, pcf8575_handle_t *pcf_handle);

/**
 * @brief Đặt hướng di chuyển xe (ghi qua PCF8575)
 *
 * @param sys       Con trỏ tới system handle
 * @param direction Giá trị PCF8575 (CAR_FORWARD, CAR_BACKWARD, ...)
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t tb6612_set_direction(tb6612_system_t *sys, uint8_t direction);

/**
 * @brief Đặt tốc độ PWM cho tất cả 4 motor
 *
 * @param duty  Giá trị duty (0-255)
 */
void tb6612_set_speed_all(uint8_t duty);

/**
 * @brief Dừng tất cả motor (direction = 0x00)
 */
esp_err_t tb6612_stop_all(tb6612_system_t *sys);

/**
 * @brief Lấy giá trị encoder count
 *
 * @param sys   Con trỏ tới system handle
 * @param id    ID encoder (0-3)
 * @param count Con trỏ nhận giá trị đếm
 * @return esp_err_t
 */
esp_err_t tb6612_encoder_get_count(tb6612_system_t *sys, encoder_id_t id, int *count);

/**
 * @brief Reset tất cả encoder về 0
 */
esp_err_t tb6612_encoder_clear_all(tb6612_system_t *sys);

/**
 * @brief Tạo task in giá trị encoder ra monitor (mỗi 500ms)
 */
void tb6612_encoder_start_print_task(tb6612_system_t *sys);

#ifdef __cplusplus
}
#endif

#endif /* TB6612FNG_H */
