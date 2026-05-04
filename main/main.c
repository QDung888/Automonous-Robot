#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c.h"
#include "pcf8575.h"
#include "TB6612FNG.h"
#include "joystick.h"
#include "serial_ros.h"

static const char *TAG = "MAIN";

/* Callback khi nhận lệnh motor từ Raspberry Pi */
static tb6612_system_t *s_motor_ptr = NULL;

static void on_pi_command(uint8_t direction, uint8_t speed)
{
    if (s_motor_ptr == NULL) return;
    tb6612_set_speed_all(speed);
    tb6612_set_direction(s_motor_ptr, direction);
    ESP_LOGI(TAG, "Pi CMD: dir=0x%02X speed=%d", direction, speed);
}

void app_main(void)
{
    ESP_LOGI(TAG, "===== Robot Car – PS2 + SerialROS =====");

    /* ---------- 1. Khởi tạo I2C bus ---------- */
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_init(&bus_handle));

    /* ---------- 2. Khởi tạo PCF8575 ---------- */
    pcf8575_handle_t pcf = PCF8575_HANDLE_DEFAULT();
    ESP_ERROR_CHECK(pcf8575_init(&pcf, bus_handle));

    /* ---------- 3. Khởi tạo TB6612 system (STBY + PWM + Encoder) ---------- */
    tb6612_system_t motor_sys = {0};
    ESP_ERROR_CHECK(tb6612_init(&motor_sys, &pcf));
    s_motor_ptr = &motor_sys;

    /* Bắt đầu task in encoder */
    tb6612_encoder_start_print_task(&motor_sys);

    /* Tất cả output LOW ban đầu */
    ESP_ERROR_CHECK(tb6612_stop_all(&motor_sys));
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Bật PWM – tốc độ 70/255 */
    tb6612_set_speed_all(70);

    /* ---------- 4. Khởi tạo SerialROS (UART0 → CP2102 → Pi) ---------- */
    ESP_ERROR_CHECK(serial_ros_init());
    serial_ros_start(&motor_sys, on_pi_command);

    /* ---------- 5. Khởi tạo tay cầm PS2 ---------- */
    joystick_init();

    /* ---------- 6. Bắt đầu task điều khiển xe bằng PS2 ---------- */
    joystick_start_control_task(&motor_sys);

    ESP_LOGI(TAG, "Sẵn sàng – PS2 + SerialROS encoder data → Pi");

    /* Main task không cần làm gì thêm */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
