#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2c.h"
#include "pcf8575.h"
#include "TB6612FNG.h"
#include "joystick.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "===== Robot Car – PS2 Controller =====");

    /* ---------- 1. Khởi tạo I2C bus ---------- */
    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_init(&bus_handle));

    /* ---------- 2. Khởi tạo PCF8575 ---------- */
    pcf8575_handle_t pcf = PCF8575_HANDLE_DEFAULT();
    ESP_ERROR_CHECK(pcf8575_init(&pcf, bus_handle));

    /* ---------- 3. Khởi tạo TB6612 system (STBY + PWM + Encoder) ---------- */
    tb6612_system_t motor_sys = {0};
    ESP_ERROR_CHECK(tb6612_init(&motor_sys, &pcf));

    /* Bắt đầu task in encoder */
    tb6612_encoder_start_print_task(&motor_sys);

    /* Tất cả output LOW ban đầu */
    ESP_ERROR_CHECK(tb6612_stop_all(&motor_sys));
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Bật PWM – tốc độ 70/255 */
    tb6612_set_speed_all(70);

    /* ---------- 4. Khởi tạo tay cầm PS2 ---------- */
    joystick_init();

    /* ---------- 5. Bắt đầu task điều khiển xe bằng PS2 ---------- */
    joystick_start_control_task(&motor_sys);

    ESP_LOGI(TAG, "Sẵn sàng – dùng D-pad hoặc analog stick trái để điều khiển xe");

    /* Main task không cần làm gì thêm */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
