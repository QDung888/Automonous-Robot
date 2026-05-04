/**
 * @file vl53l1x.c
 * @brief Driver VL53L1X – khởi tạo 2 cảm biến + đọc khoảng cách
 *
 * Quy trình XSHUT sequencing (đổi I2C address):
 *   1. Cả 2 XSHUT LOW (reset cả 2)
 *   2. XSHUT1 HIGH → sensor1 boot ở 0x29 → đổi thành 0x30
 *   3. XSHUT2 HIGH → sensor2 boot ở 0x29
 *   4. Init + StartRanging cả 2
 */

#include "vl53l1x.h"
#include "VL53L1X_api.h"
#include "VL53L1X_calibration.h"
#include "vl53l1_platform.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "VL53L1X";

/* dev = I2C address cho mỗi sensor */
static uint16_t s_dev[VL53L1X_SENSOR_COUNT] = {0};
static bool s_initialized = false;

/* ==================== Helpers ==================== */

static void xshut_gpio_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << VL53L1X_XSHUT_1_GPIO) | (1ULL << VL53L1X_XSHUT_2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    /* Cả 2 XSHUT LOW → reset */
    gpio_set_level(VL53L1X_XSHUT_1_GPIO, 0);
    gpio_set_level(VL53L1X_XSHUT_2_GPIO, 0);
    ESP_LOGI(TAG, "XSHUT GPIOs init – cả 2 sensor đang reset");
}

static bool wait_sensor_boot(uint16_t dev, int timeout_ms)
{
    uint8_t boot_state = 0;
    int elapsed = 0;
    while (boot_state == 0 && elapsed < timeout_ms) {
        VL53L1X_BootState(dev, &boot_state);
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    return (boot_state != 0);
}

static bool init_one_sensor(uint16_t dev, const char *name)
{
    /* Chờ boot */
    if (!wait_sensor_boot(dev, 2000)) {
        ESP_LOGE(TAG, "%s: boot timeout!", name);
        return false;
    }
    ESP_LOGI(TAG, "%s: booted", name);

    /* Sensor init */
    if (VL53L1X_SensorInit(dev) != 0) {
        ESP_LOGE(TAG, "%s: SensorInit failed!", name);
        return false;
    }

    /* Cấu hình: Long distance mode */
    VL53L1X_SetDistanceMode(dev, VL53L1X_DIST_LONG);

    /* Calibration nhiệt độ */
    VL53L1X_StartTemperatureUpdate(dev);

    /* Bắt đầu đo */
    VL53L1X_StartRanging(dev);

    /* Log info */
    uint16_t sensor_id = 0;
    VL53L1X_GetSensorId(dev, &sensor_id);
    ESP_LOGI(TAG, "%s: ready (addr=0x%02X, model=0x%04X)", name, dev, sensor_id);

    return true;
}

/* ==================== Public API ==================== */

esp_err_t vl53l1x_dual_init(i2c_master_bus_handle_t bus_handle)
{
    ESP_LOGI(TAG, "=== Khởi tạo 2 cảm biến VL53L1X ===");

    /* 1. Setup platform I2C bus */
    vl53l1_platform_set_bus(bus_handle);

    /* 2. Init XSHUT GPIOs – reset cả 2 sensor */
    xshut_gpio_init();
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 3. Bật sensor 1 (XSHUT HIGH) → boot ở 0x29 */
    gpio_set_level(VL53L1X_XSHUT_1_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Thêm device ở default address */
    uint16_t dev1 = vl53l1_platform_add_device(VL53L1X_DEFAULT_ADDR);
    if (dev1 == 0) {
        ESP_LOGE(TAG, "Sensor 1: add device failed");
        return ESP_FAIL;
    }

    /* Chờ boot */
    if (!wait_sensor_boot(dev1, 2000)) {
        ESP_LOGE(TAG, "Sensor 1: boot timeout at default addr");
        return ESP_FAIL;
    }

    /* Đổi address sensor 1: 0x29 → 0x30 */
    ESP_LOGI(TAG, "Sensor 1: đổi address 0x%02X → 0x%02X",
             VL53L1X_DEFAULT_ADDR, VL53L1X_SENSOR1_ADDR);
    VL53L1X_SetI2CAddress(dev1, VL53L1X_SENSOR1_ADDR);
    vl53l1_platform_change_address(VL53L1X_DEFAULT_ADDR, VL53L1X_SENSOR1_ADDR);
    s_dev[VL53L1X_SENSOR_1] = VL53L1X_SENSOR1_ADDR;
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 4. Bật sensor 2 (XSHUT HIGH) → boot ở 0x29 */
    gpio_set_level(VL53L1X_XSHUT_2_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint16_t dev2 = vl53l1_platform_add_device(VL53L1X_SENSOR2_ADDR);
    if (dev2 == 0) {
        ESP_LOGE(TAG, "Sensor 2: add device failed");
        return ESP_FAIL;
    }
    s_dev[VL53L1X_SENSOR_2] = VL53L1X_SENSOR2_ADDR;

    /* 5. Init cả 2 sensor */
    if (!init_one_sensor(s_dev[VL53L1X_SENSOR_1], "Sensor1")) {
        return ESP_FAIL;
    }
    if (!init_one_sensor(s_dev[VL53L1X_SENSOR_2], "Sensor2")) {
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "=== 2 VL53L1X sẵn sàng (S1=0x%02X, S2=0x%02X) ===",
             VL53L1X_SENSOR1_ADDR, VL53L1X_SENSOR2_ADDR);
    return ESP_OK;
}

uint16_t vl53l1x_read_distance(vl53l1x_sensor_id_t sensor_id)
{
    if (!s_initialized || sensor_id >= VL53L1X_SENSOR_COUNT) return 0;

    uint16_t distance = 0;
    uint8_t data_ready = 0;

    /* Chờ data ready (timeout 100ms) */
    for (int i = 0; i < 50; i++) {
        VL53L1X_CheckForDataReady(s_dev[sensor_id], &data_ready);
        if (data_ready) break;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    if (data_ready) {
        VL53L1X_GetDistance(s_dev[sensor_id], &distance);
        VL53L1X_ClearInterrupt(s_dev[sensor_id]);
    }

    return distance;
}

/* ==================== Print task ==================== */

static void vl53l1x_print_task(void *arg)
{
    ESP_LOGI(TAG, "Distance print task started");

    while (1) {
        uint16_t d1 = vl53l1x_read_distance(VL53L1X_SENSOR_1);
        uint16_t d2 = vl53l1x_read_distance(VL53L1X_SENSOR_2);

        ESP_LOGI("VL53L1X", "S1: %4d mm  |  S2: %4d mm", d1, d2);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void vl53l1x_start_print_task(void)
{
    xTaskCreate(vl53l1x_print_task, "vl53_print", 4096, NULL, 4, NULL);
}
