#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "vl53l1x.h"
#include "VL53L1X_api.h"
#include "esp32_bno08x.h"

static const char *TAG_VL53 = "VL53L1X_TEST";
static const char *TAG_BNO = "BNO08X_TEST";
#define VL53L1X_INT_GPIO GPIO_NUM_38

static TaskHandle_t s_data_ready_task = NULL;

static void IRAM_ATTR vl53l1x_int_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_data_ready_task, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}

static void bno08x_test_task(void *pvParameters)
{
    ESP_LOGI(TAG_BNO, "Bắt đầu kiểm tra cảm biến BNO080/BNO085...");

    esp32_bno08x_config_t imu_config = {
        .spi_peripheral = SPI2_HOST,  // HSPI_HOST -> SPI2_HOST
        .io_mosi = GPIO_NUM_11,  // MOSI -> IO11
        .io_miso = GPIO_NUM_13,  // MISO -> IO13
        .io_sclk = GPIO_NUM_12,  // SCK -> IO12
        .io_cs = GPIO_NUM_10,    // CS -> IO10
        .io_int = GPIO_NUM_1,    // INT -> IO1
        .io_rst = GPIO_NUM_2,    // RST -> IO2
        .sclk_speed = 2000000,
        .install_isr_service = false,  // ISR service đã được cài đặt bởi VL53L1X
    };

    ESP_LOGI(TAG_BNO, "Tạo handle BNO08x...");
    esp32_bno08x_handle_t *imu = esp32_bno08x_create(&imu_config);
    if (imu == NULL) {
        ESP_LOGE(TAG_BNO, "Tạo handle BNO08x thất bại");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG_BNO, "Handle BNO08x đã tạo thành công");

    ESP_LOGI(TAG_BNO, "Khởi tạo BNO08x...");
    if (!esp32_bno08x_initialize(imu)) {
        ESP_LOGE(TAG_BNO, "Khởi tạo BNO08x thất bại");
        esp32_bno08x_destroy(imu);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG_BNO, "BNO08x đã khởi tạo xong");

    esp32_bno08x_reset_reason_t reset_reason = esp32_bno08x_get_reset_reason(imu);
    ESP_LOGI(TAG_BNO, "Reset reason: %d", reset_reason);

    ESP_LOGI(TAG_BNO, "Bật BNO08x...");
    if (!esp32_bno08x_on(imu)) {
        ESP_LOGW(TAG_BNO, "Không bật được BNO08x (on())");
    } else {
        ESP_LOGI(TAG_BNO, "BNO08x đã bật thành công");
    }

    ESP_LOGI(TAG_BNO, "Bật game rotation vector và calibrated gyro...");
    if (!esp32_bno08x_enable_game_rotation_vector(imu, 100000)) {
        ESP_LOGW(TAG_BNO, "Không bật game rotation vector");
    }
    if (!esp32_bno08x_enable_calibrated_gyro(imu, 100000)) {
        ESP_LOGW(TAG_BNO, "Không bật calibrated gyro");
    }
    if (!esp32_bno08x_enable_linear_acceleration(imu, 100000)) {
        ESP_LOGW(TAG_BNO, "Không bật linear acceleration");
    }

    ESP_LOGI(TAG_BNO, "Bắt đầu vòng lặp đọc dữ liệu...");
    const float ACCEL_THRESHOLD = 0.5f; // m/s^2
    const float GYRO_THRESHOLD = 0.2f;  // rad/s
    while (1) {
        if (esp32_bno08x_data_available(imu)) {
            if (esp32_bno08x_has_game_rotation_vector(imu)) {
                float roll = 0, pitch = 0, yaw = 0;
                if (esp32_bno08x_get_game_rotation_vector(imu, &roll, &pitch, &yaw)) {
                    ESP_LOGI(TAG_BNO, "Game RV: roll=%.2f pitch=%.2f yaw=%.2f", roll, pitch, yaw);
                }
            }

            if (esp32_bno08x_has_calibrated_gyro(imu)) {
                float gx = 0, gy = 0, gz = 0;
                if (esp32_bno08x_get_calibrated_gyro(imu, &gx, &gy, &gz)) {
                    // ESP_LOGI(TAG_BNO, "Cal Gyro: x=%.3f y=%.3f z=%.3f", gx, gy, gz);
                    if (gz > GYRO_THRESHOLD) {
                        ESP_LOGI(TAG_BNO, ">>> Đang XOAY TRÁI (gz=%.2f)", gz);
                    } else if (gz < -GYRO_THRESHOLD) {
                        ESP_LOGI(TAG_BNO, ">>> Đang XOAY PHẢI (gz=%.2f)", gz);
                    }
                }
            }

            if (esp32_bno08x_has_linear_acceleration(imu)) {
                float ax = 0, ay = 0, az = 0;
                if (esp32_bno08x_get_linear_acceleration(imu, &ax, &ay, &az)) {
                    // ESP_LOGI(TAG_BNO, "Linear Accel: x=%.3f y=%.3f z=%.3f", ax, ay, az);
                    if (ax > ACCEL_THRESHOLD) {
                        ESP_LOGI(TAG_BNO, ">>> Đang TIẾN (ax=%.2f)", ax);
                    } else if (ax < -ACCEL_THRESHOLD) {
                        ESP_LOGI(TAG_BNO, ">>> Đang LÙI (ax=%.2f)", ax);
                    }
                }
            }
        } else {
            ESP_LOGI(TAG_BNO, "Chưa có dữ liệu BNO08x, chờ 100ms...");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


static bool setup_vl53l1x_interrupt(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << VL53L1X_INT_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG_VL53, "Failed to configure interrupt GPIO %d", VL53L1X_INT_GPIO);
        return false;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_VL53, "Failed to install ISR service: %s", esp_err_to_name(err));
        return false;
    }

    if (gpio_isr_handler_add(VL53L1X_INT_GPIO, vl53l1x_int_isr, NULL) != ESP_OK) {
        ESP_LOGE(TAG_VL53, "Failed to attach ISR handler");
        return false;
    }

    return true;
}

static bool configure_vl53l1x_data_ready_interrupt(uint16_t dev)
{
    if (VL53L1X_SetInterruptPolarity(dev, 1) != 0) {
        ESP_LOGE(TAG_VL53, "Failed to set VL53L1X interrupt polarity");
        return false;
    }

    if (VL53L1_WrByte(dev, SYSTEM__INTERRUPT_CONFIG_GPIO, 0x20) != 0) {
        ESP_LOGE(TAG_VL53, "Failed to configure VL53L1X data-ready interrupt");
        return false;
    }

    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG_VL53, "Bắt đầu kiểm tra cảm biến VL53L1X...");

    s_data_ready_task = xTaskGetCurrentTaskHandle();

    if (!setup_vl53l1x_interrupt()) {
        ESP_LOGE(TAG_VL53, "Không thể cấu hình ngắt cho VL53L1X");
        return;
    }

    // 1. Khởi tạo cấu hình I2C
    vl53l1x_i2c_handle_t i2c_handle = VL53L1X_I2C_INIT;
    i2c_handle.i2c_port = I2C_NUM_0;

    // 2. Khởi tạo handle tổng của component
    vl53l1x_handle_t vl53l1x_handle = VL53L1X_INIT;
    vl53l1x_handle.i2c_handle = &i2c_handle;

    if (!vl53l1x_init(&vl53l1x_handle)) {
        ESP_LOGE(TAG_VL53, "Khởi tạo I2C Master thất bại!");
        return;
    }

    // 3. Cấu hình và thêm thiết bị (cảm biến)
    vl53l1x_device_handle_t sensor = VL53L1X_DEVICE_INIT;
    sensor.vl53l1x_handle = &vl53l1x_handle;
    sensor.i2c_address = VL53L1X_DEFAULT_I2C_ADDRESS; // Mặc định là 0x29
    sensor.interrupt_gpio = VL53L1X_INT_GPIO;

    if (!vl53l1x_add_device(&sensor)) {
        ESP_LOGE(TAG_VL53, "Không tìm thấy cảm biến VL53L1X tại địa chỉ 0x%02X", sensor.i2c_address);
        return;
    }

    if (!configure_vl53l1x_data_ready_interrupt(sensor.dev)) {
        ESP_LOGE(TAG_VL53, "Không thể kích hoạt interrupt data-ready trên VL53L1X");
        return;
    }

    // ESP_LOGI(TAG_VL53, "Cảm biến VL53L1X đã sẵn sàng! Đi vào chờ interrupt IO38...");

    // Tạo task riêng cho BNO08x
    xTaskCreate(bno08x_test_task, "bno08x_task", 4096, NULL, 5, NULL);

    // Vòng lặp chính cho VL53L1X (ẩn log để xem BNO080)
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == 0) {
            continue;
        }

        uint16_t distance = vl53l1x_get_mm(&sensor);
        if (distance > 0) {
            // ESP_LOGI(TAG_VL53, "Khoảng cách đo được: %u mm", distance);
        } else {
            // ESP_LOGW(TAG_VL53, "Lỗi đọc dữ liệu hoặc vượt quá dải đo!");
        }
    }
}
