#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "BNO08x.hpp"

static const char *TAG = "BNO08X_App";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting BNO08x test application...");

    // Create BNO08x IMU object with custom configuration
    bno08x_config_t imu_config;
    imu_config.spi_peripheral = SPI2_HOST;
    imu_config.io_mosi = GPIO_NUM_11;
    imu_config.io_miso = GPIO_NUM_13;
    imu_config.io_sclk = GPIO_NUM_12;
    imu_config.io_cs = GPIO_NUM_10;
    imu_config.io_int = GPIO_NUM_1;
    imu_config.io_rst = GPIO_NUM_2;
    imu_config.sclk_speed = 2000000;
    imu_config.install_isr_service = true;  // Install ISR service for HINT pin

    BNO08x imu(imu_config);

    // Initialize IMU
    ESP_LOGI(TAG, "Initializing BNO08x...");
    if (!imu.initialize())
    {
        ESP_LOGE(TAG, "BNO08x initialization failed!");
        return;
    }
    ESP_LOGI(TAG, "BNO08x initialized successfully!");

    // Get and display product IDs
    sh2_ProductIds_t product_ids = imu.get_product_IDs();
    ESP_LOGI(TAG, "BNO08x Product IDs:");
    if (product_ids.numEntries > 0)
    {
        ESP_LOGI(TAG, "  SW Version Major: 0x%02x", product_ids.entry[0].swVersionMajor);
        ESP_LOGI(TAG, "  SW Version Minor: 0x%02x", product_ids.entry[0].swVersionMinor);
        ESP_LOGI(TAG, "  SW Build Number: 0x%08x", product_ids.entry[0].swBuildNumber);
        ESP_LOGI(TAG, "  SW Version Patch: 0x%04x", product_ids.entry[0].swVersionPatch);
        ESP_LOGI(TAG, "  Part Number: 0x%08x", product_ids.entry[0].swPartNumber);
    }

    // Enable desired reports
    ESP_LOGI(TAG, "Enabling sensor reports...");
    imu.rpt.rv_game.enable(100000UL);       // Game rotation vector at 100ms interval
    imu.rpt.cal_gyro.enable(100000UL);      // Calibrated gyro at 100ms interval
    ESP_LOGI(TAG, "Sensor reports enabled!");

    // Polling loop
    ESP_LOGI(TAG, "Starting polling loop...");
    while (1)
    {
        // Block until new report is detected
        if (imu.data_available())
        {
            // Check for game rotation vector report
            if (imu.rpt.rv_game.has_new_data())
            {
                // Get absolute heading in degrees (euler angles)
                bno08x_euler_angle_t euler = imu.rpt.rv_game.get_euler();
                ESP_LOGI(TAG, "Game RV - Roll: %.2f  Pitch: %.2f  Yaw: %.2f",
                         euler.x, euler.y, euler.z);
            }

            // Check for calibrated gyro report
            if (imu.rpt.cal_gyro.has_new_data())
            {
                // Get angular velocity in rad/s
                bno08x_gyro_t velocity = imu.rpt.cal_gyro.get();
                ESP_LOGW(TAG, "Cal Gyro - X: %.2f  Y: %.2f  Z: %.2f rad/s",
                         velocity.x, velocity.y, velocity.z);
            }
        }
    }
}
