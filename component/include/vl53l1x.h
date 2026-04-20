#ifndef VL53L1X_H
#define VL53L1X_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// Types consolidated from vl53l1x_types.h
typedef i2c_master_dev_handle_t dev_handle_t;

typedef struct vl53l1x_i2c_handle_s
{
    i2c_port_num_t i2c_port;
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    bool initialized;
    i2c_master_bus_handle_t bus_handle;
} vl53l1x_i2c_handle_t;

typedef struct vl53l1x_handle_s
{
    bool initialized;
    vl53l1x_i2c_handle_t *i2c_handle;
} vl53l1x_handle_t;

typedef struct vl53l1x_device_handle_s
{
    vl53l1x_handle_t *vl53l1x_handle;
    gpio_num_t xshut_gpio;
    uint8_t i2c_address;
    gpio_num_t interrupt_gpio;
    uint32_t scl_speed_hz;

    dev_handle_t dev_handle;
    uint16_t dev;
} vl53l1x_device_handle_t;

typedef enum
{
    SHORT = 1, // <1.3m
    LONG = 2,  // <4m
} distance_mode;

// Initialization constants
static const uint8_t VL53L1X_DEFAULT_I2C_ADDRESS = 0x29;

#define VL53L1X_INIT { \
    .initialized = false, \
    .i2c_handle = NULL, \
}

#define VL53L1X_DEVICE_INIT { \
    .vl53l1x_handle = NULL, \
    .xshut_gpio = GPIO_NUM_NC, \
    .i2c_address = VL53L1X_DEFAULT_I2C_ADDRESS, \
    .interrupt_gpio = GPIO_NUM_NC, \
    .scl_speed_hz = 400000, \
    .dev_handle = NULL, \
    .dev = 0, \
}

#define VL53L1X_I2C_INIT { \
    .i2c_port = I2C_NUM_0, \
    .sda_gpio = GPIO_NUM_NC, \
    .scl_gpio = GPIO_NUM_NC, \
    .initialized = false, \
    .bus_handle = NULL, \
}

// Function prototypes
bool vl53l1x_init(vl53l1x_handle_t *vl53l1x_handle);
bool vl53l1x_add_device(vl53l1x_device_handle_t *device);
bool vl53l1x_update_device_address(vl53l1x_device_handle_t *device, uint8_t new_address);

uint16_t vl53l1x_get_mm(const vl53l1x_device_handle_t *device);

void vl53l1x_log_sensor_id(const vl53l1x_device_handle_t *device);
void vl53l1x_log_ambient_light(const vl53l1x_device_handle_t *device);
void vl53l1x_log_signal_rate(const vl53l1x_device_handle_t *device);

#endif
