/**
 * @file vl53l1_platform.c
 * @brief VL53L1X ULD Platform layer – ESP-IDF I2C master driver
 *
 * `dev` (uint16_t) = I2C 7-bit address.
 * Platform layer duy trì bảng mapping address → i2c_master_dev_handle_t.
 *
 * Copyright (c) 2023 STMicroelectronics. Licensed AS-IS.
 */

#include "vl53l1_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "VL53L1_PLAT";

/* ==================== Device table ==================== */

#define VL53L1_MAX_DEVICES  4

typedef struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev_handle;
    bool active;
} vl53l1_dev_entry_t;

static vl53l1_dev_entry_t s_dev_table[VL53L1_MAX_DEVICES] = {0};
static i2c_master_bus_handle_t s_bus = NULL;

/* Đăng ký bus I2C (gọi 1 lần) */
void vl53l1_platform_set_bus(i2c_master_bus_handle_t bus)
{
    s_bus = bus;
}

/* Thêm device vào bảng, trả về dev (= address) */
uint16_t vl53l1_platform_add_device(uint8_t i2c_addr)
{
    /* Kiểm tra đã có chưa */
    for (int i = 0; i < VL53L1_MAX_DEVICES; i++) {
        if (s_dev_table[i].active && s_dev_table[i].addr == i2c_addr) {
            return i2c_addr;    /* Đã có rồi */
        }
    }

    /* Tìm slot trống */
    for (int i = 0; i < VL53L1_MAX_DEVICES; i++) {
        if (!s_dev_table[i].active) {
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = i2c_addr,
                .scl_speed_hz = 400000,
            };
            esp_err_t err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev_table[i].dev_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Add device 0x%02X failed: %s", i2c_addr, esp_err_to_name(err));
                return 0;
            }
            s_dev_table[i].addr = i2c_addr;
            s_dev_table[i].active = true;
            ESP_LOGI(TAG, "Device 0x%02X added (slot %d)", i2c_addr, i);
            return i2c_addr;
        }
    }

    ESP_LOGE(TAG, "Device table full!");
    return 0;
}

/* Xóa device cũ, thêm device mới (dùng khi đổi address) */
uint16_t vl53l1_platform_change_address(uint8_t old_addr, uint8_t new_addr)
{
    for (int i = 0; i < VL53L1_MAX_DEVICES; i++) {
        if (s_dev_table[i].active && s_dev_table[i].addr == old_addr) {
            /* Xóa device handle cũ */
            i2c_master_bus_rm_device(s_dev_table[i].dev_handle);
            s_dev_table[i].active = false;

            /* Thêm device mới với address mới */
            return vl53l1_platform_add_device(new_addr);
        }
    }
    ESP_LOGE(TAG, "Device 0x%02X not found for address change", old_addr);
    return 0;
}

/* Tìm device handle theo address */
static i2c_master_dev_handle_t find_dev_handle(uint16_t dev)
{
    uint8_t addr = (uint8_t)(dev & 0x7F);
    for (int i = 0; i < VL53L1_MAX_DEVICES; i++) {
        if (s_dev_table[i].active && s_dev_table[i].addr == addr) {
            return s_dev_table[i].dev_handle;
        }
    }
    ESP_LOGE(TAG, "No handle for dev 0x%02X", addr);
    return NULL;
}

/* ==================== Platform I/O ==================== */

int8_t VL53L1_WriteMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    i2c_master_dev_handle_t h = find_dev_handle(dev);
    if (!h) return -1;

    /* Gộp [index_hi, index_lo, data...] */
    uint8_t buf[count + 2];
    buf[0] = (uint8_t)(index >> 8);
    buf[1] = (uint8_t)(index & 0xFF);
    for (uint32_t i = 0; i < count; i++) buf[i + 2] = pdata[i];

    esp_err_t err = i2c_master_transmit(h, buf, count + 2, 100);
    return (err == ESP_OK) ? 0 : -1;
}

int8_t VL53L1_ReadMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    i2c_master_dev_handle_t h = find_dev_handle(dev);
    if (!h) return -1;

    uint8_t reg[2] = { (uint8_t)(index >> 8), (uint8_t)(index & 0xFF) };
    esp_err_t err = i2c_master_transmit_receive(h, reg, 2, pdata, count, 100);
    return (err == ESP_OK) ? 0 : -1;
}

int8_t VL53L1_WrByte(uint16_t dev, uint16_t index, uint8_t data)
{
    return VL53L1_WriteMulti(dev, index, &data, 1);
}

int8_t VL53L1_WrWord(uint16_t dev, uint16_t index, uint16_t data)
{
    uint8_t buf[2] = { (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
    return VL53L1_WriteMulti(dev, index, buf, 2);
}

int8_t VL53L1_WrDWord(uint16_t dev, uint16_t index, uint32_t data)
{
    uint8_t buf[4] = {
        (uint8_t)((data >> 24) & 0xFF),
        (uint8_t)((data >> 16) & 0xFF),
        (uint8_t)((data >> 8) & 0xFF),
        (uint8_t)(data & 0xFF),
    };
    return VL53L1_WriteMulti(dev, index, buf, 4);
}

int8_t VL53L1_RdByte(uint16_t dev, uint16_t index, uint8_t *pdata)
{
    return VL53L1_ReadMulti(dev, index, pdata, 1);
}

int8_t VL53L1_RdWord(uint16_t dev, uint16_t index, uint16_t *pdata)
{
    uint8_t buf[2];
    int8_t ret = VL53L1_ReadMulti(dev, index, buf, 2);
    *pdata = ((uint16_t)buf[0] << 8) | buf[1];
    return ret;
}

int8_t VL53L1_RdDWord(uint16_t dev, uint16_t index, uint32_t *pdata)
{
    uint8_t buf[4];
    int8_t ret = VL53L1_ReadMulti(dev, index, buf, 4);
    *pdata = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
           | ((uint32_t)buf[2] << 8) | buf[3];
    return ret;
}

int8_t VL53L1_WaitMs(uint16_t dev, int32_t wait_ms)
{
    (void)dev;
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    return 0;
}

/* Không dùng function pointers nữa – giữ lại cho backward compatibility */
vl53l1x_write_multi g_vl53l1x_write_multi_ptr = NULL;
vl53l1x_read_multi g_vl53l1x_read_multi_ptr = NULL;
vl53l1x_write_byte g_vl53l1x_write_byte_ptr = NULL;
vl53l1x_write_word g_vl53l1x_write_word_ptr = NULL;
vl53l1x_write_dword g_vl53l1x_write_dword_ptr = NULL;
vl53l1x_read_byte g_vl53l1x_read_byte_ptr = NULL;
vl53l1x_read_word g_vl53l1x_read_word_ptr = NULL;
vl53l1x_read_dword g_vl53l1x_read_dword_ptr = NULL;
