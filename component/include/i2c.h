#ifndef I2C_H
#define I2C_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "vl53l1x.h"

#define I2C_DEFAULT_PORT I2C_NUM_0
#define I2C_SCL_IO GPIO_NUM_40
#define I2C_SDA_IO GPIO_NUM_41

// Forward declaration to avoid circular dependency
struct vl53l1x_i2c_handle_s;
struct vl53l1x_device_handle_s;

typedef i2c_master_dev_handle_t dev_handle_t;

bool i2c_master_init(struct vl53l1x_i2c_handle_s *i2c_handle);
bool i2c_add_device(struct vl53l1x_device_handle_s *device);
bool i2c_update_address(const uint16_t dev, const uint8_t new_address);

uint8_t i2c_find_first_addr(struct vl53l1x_i2c_handle_s *i2c_handle);

bool i2c_write(const dev_handle_t *device, uint16_t reg, uint8_t value);
bool i2c_read(const dev_handle_t *device, uint16_t reg, uint8_t *buf, size_t len);

int8_t i2c_write_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);
int8_t i2c_read_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count);
int8_t i2c_write_dword(uint16_t dev, uint16_t index, uint32_t data);
int8_t i2c_write_word(uint16_t dev, uint16_t index, uint16_t data);
int8_t i2c_write_byte(uint16_t dev, uint16_t index, uint8_t data);
int8_t i2c_read_byte(uint16_t dev, uint16_t index, uint8_t *pdata);
int8_t i2c_read_word(uint16_t dev, uint16_t index, uint16_t *pdata);
int8_t i2c_read_dword(uint16_t dev, uint16_t index, uint32_t *pdata);

// Device handler internal structures and functions
typedef struct
{
    uint16_t dev;
    dev_handle_t dev_handle;
} device_key_t;

typedef struct
{
    device_key_t *keys;
    uint8_t count;
} device_map_t;

void deinit_device_map();
void add_device_key(const uint16_t dev, const dev_handle_t handle);
int8_t remove_device_key(const uint16_t address);
void change_device_address(const uint16_t dev, const uint8_t new_address);
dev_handle_t get_device_handle(const uint16_t dev);

uint16_t create_dev(const i2c_port_num_t port, const uint8_t address);
uint8_t get_address(const uint16_t dev);
i2c_port_num_t get_port(const uint16_t dev);

#endif
