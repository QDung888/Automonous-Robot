#include "esp32_bno08x.h"
#include "BNO08x.hpp"
#include <stdlib.h>

struct esp32_bno08x_handle_t {
    BNO08x* impl;
};

static BNO08xResetReason to_cpp_reset_reason(esp32_bno08x_reset_reason_t reason)
{
    switch (reason) {
        case ESP32_BNO08X_RESET_REASON_POR:
            return BNO08xResetReason::POR;
        case ESP32_BNO08X_RESET_REASON_INT_RST:
            return BNO08xResetReason::INT_RST;
        case ESP32_BNO08X_RESET_REASON_WTD:
            return BNO08xResetReason::WTD;
        case ESP32_BNO08X_RESET_REASON_EXT_RST:
            return BNO08xResetReason::EXT_RST;
        case ESP32_BNO08X_RESET_REASON_OTHER:
            return BNO08xResetReason::OTHER;
        default:
            return BNO08xResetReason::UNDEFINED;
    }
}

static esp32_bno08x_reset_reason_t from_cpp_reset_reason(BNO08xResetReason reason)
{
    switch (reason) {
        case BNO08xResetReason::POR:
            return ESP32_BNO08X_RESET_REASON_POR;
        case BNO08xResetReason::INT_RST:
            return ESP32_BNO08X_RESET_REASON_INT_RST;
        case BNO08xResetReason::WTD:
            return ESP32_BNO08X_RESET_REASON_WTD;
        case BNO08xResetReason::EXT_RST:
            return ESP32_BNO08X_RESET_REASON_EXT_RST;
        case BNO08xResetReason::OTHER:
            return ESP32_BNO08X_RESET_REASON_OTHER;
        default:
            return ESP32_BNO08X_RESET_REASON_UNDEFINED;
    }
}

esp32_bno08x_handle_t* esp32_bno08x_create(const esp32_bno08x_config_t* config)
{
    bno08x_config_t cpp_config;
    if (config) {
        cpp_config = bno08x_config_t(
            config->spi_peripheral,
            config->io_mosi,
            config->io_miso,
            config->io_sclk,
            config->io_cs,
            config->io_int,
            config->io_rst,
            config->sclk_speed,
            config->install_isr_service
        );
    } else {
        cpp_config = bno08x_config_t();
    }

    esp32_bno08x_handle_t* handle = (esp32_bno08x_handle_t*)malloc(sizeof(esp32_bno08x_handle_t));
    if (!handle) return NULL;

    handle->impl = new BNO08x(cpp_config);
    return handle;
}

void esp32_bno08x_destroy(esp32_bno08x_handle_t* handle)
{
    if (handle) {
        delete handle->impl;
        free(handle);
    }
}

bool esp32_bno08x_initialize(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->initialize();
}

bool esp32_bno08x_hard_reset(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->hard_reset();
}

bool esp32_bno08x_soft_reset(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->soft_reset();
}

bool esp32_bno08x_disable_all_reports(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->disable_all_reports();
}

bool esp32_bno08x_on(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->on();
}

bool esp32_bno08x_sleep(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->sleep();
}

bool esp32_bno08x_data_available(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->data_available();
}

bool esp32_bno08x_has_game_rotation_vector(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->rpt.rv_game.has_new_data();
}

bool esp32_bno08x_enable_game_rotation_vector(esp32_bno08x_handle_t* handle, uint32_t period_us)
{
    return handle && handle->impl->rpt.rv_game.enable(period_us);
}

bool esp32_bno08x_get_game_rotation_vector(esp32_bno08x_handle_t* handle, float* roll, float* pitch, float* yaw)
{
    if (!handle || !handle->impl || !roll || !pitch || !yaw) return false;
    bno08x_euler_angle_t euler = handle->impl->rpt.rv_game.get_euler(true);
    *roll = euler.x;
    *pitch = euler.y;
    *yaw = euler.z;
    return true;
}

bool esp32_bno08x_has_calibrated_gyro(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->rpt.cal_gyro.has_new_data();
}

bool esp32_bno08x_enable_calibrated_gyro(esp32_bno08x_handle_t* handle, uint32_t period_us)
{
    return handle && handle->impl->rpt.cal_gyro.enable(period_us);
}

bool esp32_bno08x_get_calibrated_gyro(esp32_bno08x_handle_t* handle, float* x, float* y, float* z)
{
    if (!handle || !handle->impl || !x || !y || !z) return false;
    bno08x_gyro_t gyro = handle->impl->rpt.cal_gyro.get();
    *x = gyro.x;
    *y = gyro.y;
    *z = gyro.z;
    return true;
}

bool esp32_bno08x_has_linear_acceleration(esp32_bno08x_handle_t* handle)
{
    return handle && handle->impl->rpt.linear_accelerometer.has_new_data();
}

bool esp32_bno08x_enable_linear_acceleration(esp32_bno08x_handle_t* handle, uint32_t period_us)
{
    return handle && handle->impl->rpt.linear_accelerometer.enable(period_us);
}

bool esp32_bno08x_get_linear_acceleration(esp32_bno08x_handle_t* handle, float* x, float* y, float* z)
{
    if (!handle || !handle->impl || !x || !y || !z) return false;
    bno08x_accel_t accel = handle->impl->rpt.linear_accelerometer.get();
    *x = accel.x;
    *y = accel.y;
    *z = accel.z;
    return true;
}

bool esp32_bno08x_get_system_orientation(esp32_bno08x_handle_t* handle, float* w, float* x, float* y, float* z)
{
    if (!handle || !handle->impl) return false;
    return handle->impl->get_system_orientation(*w, *x, *y, *z);
}

bool esp32_bno08x_register_callback(esp32_bno08x_handle_t* handle, void (*cb)(void))
{
    // Note: This might need adjustment based on actual API
    return false; // Placeholder
}

esp32_bno08x_reset_reason_t esp32_bno08x_get_reset_reason(esp32_bno08x_handle_t* handle)
{
    if (!handle || !handle->impl) return ESP32_BNO08X_RESET_REASON_UNDEFINED;
    return from_cpp_reset_reason(handle->impl->get_reset_reason());
}