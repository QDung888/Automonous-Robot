/**
 * @file joystick.c
 * @brief Driver tay cầm PS2 wireless – Implementation
 *
 * Giao thức PS2:
 *   1. CS LOW → bắt đầu
 *   2. Gửi 0x01 (start) → nhận header
 *   3. Gửi 0x42 (poll)  → nhận mode (0x41/0x73/0x79)
 *   4. Gửi 0x00          → nhận 0x5A (ready)
 *   5. Gửi 0x00 x N      → nhận button/stick data
 *   6. CS HIGH → kết thúc
 */

#include "joystick.h"
#include "spi.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "JOYSTICK";

/* ==================== PS2 Commands ==================== */

/** Gửi lệnh poll và đọc dữ liệu */
static esp_err_t ps2_poll_raw(uint8_t *rx_buf, int *rx_len)
{
    /* Packet gửi: 01 42 00 00 00 00 00 00 00
     * (9 byte cho analog mode, 5 byte cho digital mode)
     * Gửi đủ 9 byte, nếu digital thì byte 5-8 sẽ là 0xFF */
    const uint8_t tx[] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t rx[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    spi_cs_low();
    spi_transfer(tx, rx, 9);
    spi_cs_high();

    /* Kiểm tra response hợp lệ */
    if (rx[1] == 0xFF || rx[2] != 0x5A) {
        *rx_len = 0;
        return ESP_ERR_NOT_FOUND;
    }

    /* Kiểm tra mode hợp lệ (0x41=digital, 0x73=analog, 0x79=analog+pressure) */
    if (rx[1] != 0x41 && rx[1] != 0x73 && rx[1] != 0x79) {
        *rx_len = 0;
        return ESP_ERR_NOT_FOUND;
    }

    /*
     * Nếu cả 2 byte button đều = 0x00 → tất cả nút nhấn cùng lúc
     * → không thể xảy ra trên thực tế → data rác → bỏ qua
     */
    if (rx[3] == 0x00 && rx[4] == 0x00) {
        *rx_len = 0;
        return ESP_ERR_NOT_FOUND;
    }

    /* Copy kết quả */
    for (int i = 0; i < 9; i++) {
        rx_buf[i] = rx[i];
    }

    /* Xác định độ dài dữ liệu dựa trên mode */
    if (rx[1] == 0x73 || rx[1] == 0x79) {
        *rx_len = 9;    /* Analog mode: 4 byte stick thêm */
    } else {
        *rx_len = 5;    /* Digital mode: chỉ 2 byte button */
    }

    return ESP_OK;
}

/** Gửi lệnh enter config mode */
static void ps2_enter_config(void)
{
    const uint8_t tx[] = {0x01, 0x43, 0x00, 0x01, 0x00};
    spi_cs_low();
    spi_transfer(tx, NULL, 5);
    spi_cs_high();
    ets_delay_us(100);
}

/** Gửi lệnh set analog mode (lock) */
static void ps2_set_analog_mode(void)
{
    const uint8_t tx[] = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    spi_cs_low();
    spi_transfer(tx, NULL, 9);
    spi_cs_high();
    ets_delay_us(100);
}

/** Gửi lệnh exit config mode */
static void ps2_exit_config(void)
{
    const uint8_t tx[] = {0x01, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    spi_cs_low();
    spi_transfer(tx, NULL, 9);
    spi_cs_high();
    ets_delay_us(100);
}

/* ==================== Public API ==================== */

esp_err_t joystick_init(void)
{
    /* Khởi tạo SPI GPIO */
    ESP_RETURN_ON_ERROR(spi_ps2_init(), TAG, "SPI init failed");

    /* Chờ controller khởi động */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Thử enter analog mode */
    for (int attempt = 0; attempt < 5; attempt++) {
        ps2_enter_config();
        vTaskDelay(pdMS_TO_TICKS(10));
        ps2_set_analog_mode();
        vTaskDelay(pdMS_TO_TICKS(10));
        ps2_exit_config();
        vTaskDelay(pdMS_TO_TICKS(50));

        /* Kiểm tra bằng cách poll */
        uint8_t rx[9];
        int len;
        if (ps2_poll_raw(rx, &len) == ESP_OK) {
            ESP_LOGI(TAG, "PS2 controller connected – mode=0x%02X (attempt %d)",
                     rx[1], attempt + 1);
            if (rx[1] == 0x73 || rx[1] == 0x79) {
                ESP_LOGI(TAG, "Analog mode OK");
            } else {
                ESP_LOGW(TAG, "Digital mode (0x%02X) – analog có thể không hỗ trợ", rx[1]);
            }
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Không tìm thấy controller, thử lại... (%d/5)", attempt + 1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGE(TAG, "Không kết nối được tay cầm PS2 sau 5 lần thử");
    /* Vẫn trả về OK để chương trình chạy tiếp, poll sẽ báo disconnected */
    return ESP_OK;
}

esp_err_t joystick_poll(ps2_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t rx[9];
    int len;
    esp_err_t err = ps2_poll_raw(rx, &len);

    if (err != ESP_OK) {
        data->connected = false;
        data->buttons1 = 0xFF;     /* Không nhấn */
        data->buttons2 = 0xFF;
        data->left_x = 128;
        data->left_y = 128;
        data->right_x = 128;
        data->right_y = 128;
        return err;
    }

    data->connected = true;
    data->mode = rx[1];
    data->buttons1 = rx[3];
    data->buttons2 = rx[4];

    if (len >= 9) {
        /* Analog mode */
        data->right_x = rx[5];
        data->right_y = rx[6];
        data->left_x = rx[7];
        data->left_y = rx[8];
    } else {
        /* Digital mode – center */
        data->right_x = 128;
        data->right_y = 128;
        data->left_x = 128;
        data->left_y = 128;
    }

    return ESP_OK;
}

/* ==================== Control Task ==================== */

/* Dead zone cho analog stick */
#define STICK_DEADZONE      30
#define STICK_CENTER        128

static tb6612_system_t *s_motor_sys = NULL;

static void joystick_control_task(void *arg)
{
    ps2_data_t ps2;
    uint8_t last_dir = CAR_STOP;
    bool was_connected = false;

    ESP_LOGI(TAG, "Joystick control task started");

    while (1) {
        joystick_poll(&ps2);

        if (!ps2.connected) {
            /* Controller mất kết nối → dừng xe ngay */
            if (last_dir != CAR_STOP) {
                ESP_LOGW(TAG, "Controller disconnected – DỪNG");
                tb6612_stop_all(s_motor_sys);
                last_dir = CAR_STOP;
            }
            was_connected = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!was_connected) {
            ESP_LOGI(TAG, "Controller connected! mode=0x%02X btn=0x%02X 0x%02X",
                     ps2.mode, ps2.buttons1, ps2.buttons2);
            was_connected = true;
        }

        /* Sử dụng 4 nút: Tam giác, Vuông, Tròn, X (nằm ở byte 2) */
        uint8_t new_dir = CAR_STOP;

        if (joystick_btn2_pressed(&ps2, PS2_BTN_TRIANGLE)) {
            new_dir = CAR_FORWARD;      /* Tam giác -> Tiến */
        } else if (joystick_btn2_pressed(&ps2, PS2_BTN_CROSS)) {
            new_dir = CAR_BACKWARD;     /* X -> Lùi */
        } else if (joystick_btn2_pressed(&ps2, PS2_BTN_SQUARE)) {
            new_dir = CAR_TURN_LEFT;    /* Vuông -> Trái */
        } else if (joystick_btn2_pressed(&ps2, PS2_BTN_CIRCLE)) {
            new_dir = CAR_TURN_RIGHT;   /* Tròn -> Phải */
        }

        /* Chỉ ghi PCF8575 khi hướng thay đổi */
        if (new_dir != last_dir) {
            tb6612_set_direction(s_motor_sys, new_dir);
            last_dir = new_dir;

            if (new_dir == CAR_STOP) {
                ESP_LOGI(TAG, ">> DỪNG");
            } else {
                const char *names[] = {"?", "TIẾN", "LÙI", "TRÁI", "PHẢI"};
                int idx = 0;
                if (new_dir == CAR_FORWARD)    idx = 1;
                if (new_dir == CAR_BACKWARD)   idx = 2;
                if (new_dir == CAR_TURN_LEFT)  idx = 3;
                if (new_dir == CAR_TURN_RIGHT) idx = 4;
                ESP_LOGI(TAG, ">> %s (0x%02X)", names[idx], new_dir);
            }
        }

        /* Poll mỗi 20ms (~50Hz) */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void joystick_start_control_task(tb6612_system_t *motor_sys)
{
    s_motor_sys = motor_sys;
    xTaskCreate(joystick_control_task, "ps2_ctrl", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "PS2 control task created");
}
