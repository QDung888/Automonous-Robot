#include "i2c.h"
#include "esp_log.h"
#include "VL53L1X_api.h"
#include <string.h>

static const char *TAG = "I2C_HANDLER";

// Device Map internal storage
static device_map_t device_map = { .keys = NULL, .count = 0 };

void deinit_device_map()
{
    if (device_map.keys) {
        free(device_map.keys);
    }
    device_map.keys = NULL;
    device_map.count = 0;
}

void add_device_key(const uint16_t dev, const dev_handle_t handle)
{
    uint8_t size = (device_map.count + 1) * sizeof(device_key_t);
    void *new_ptr = realloc(device_map.keys, size);

    if (new_ptr == NULL)
    {
        ESP_LOGE(TAG, "Failed to expand map");
        return;
    }

    device_map.keys = (device_key_t *)new_ptr;

    device_key_t key = {
        .dev = dev,
        .dev_handle = handle};

    device_map.keys[device_map.count] = key;
    device_map.count++;
}

int8_t remove_device_key(const uint16_t dev)
{
    int position = -1;
    for (uint8_t i = 0; i < device_map.count; i++)
    {
        if (device_map.keys[i].dev == dev)
        {
            position = (int)i;
            break;
        }
    }

    if (position == -1)
    {
        ESP_LOGE(TAG, "address not found");
        return 255;
    }

    if (device_map.count < 2)
    {
        deinit_device_map();
        return 0;
    }

    uint8_t elements_to_move = device_map.count - position - 1;
    if (elements_to_move > 0)
    {
        memmove(
            &device_map.keys[position],
            &device_map.keys[position + 1],
            elements_to_move * sizeof(device_key_t));
    }

    device_map.count--;

    size_t new_size = device_map.count * sizeof(device_key_t);
    void *new_ptr = realloc(device_map.keys, new_size);

    if (new_size > 0 && new_ptr == NULL)
    {
        ESP_LOGE(TAG, "Failed to shrink map memory");
    }
    else
    {
        device_map.keys = (device_key_t *)new_ptr;
    }

    return 0;
}

void change_device_address(const uint16_t dev, const uint8_t new_address)
{
    for (size_t i = 0; i < device_map.count; i++)
    {
        if (device_map.keys[i].dev == dev)
        {
            device_map.keys[i].dev = create_dev(get_port(dev), new_address);
        }
    }
}

dev_handle_t get_device_handle(const uint16_t dev)
{
    for (size_t i = 0; i < device_map.count; i++)
    {
        if (device_map.keys[i].dev == dev)
        {
            return device_map.keys[i].dev_handle;
        }
    }
    return NULL;
}

uint16_t create_dev(const i2c_port_num_t port, const uint8_t address)
{
    return ((uint16_t)port << 8) | address;
}

uint8_t get_address(const uint16_t dev)
{
    return (uint8_t)(dev & 0xFF);
}

i2c_port_num_t get_port(const uint16_t dev)
{
    return (i2c_port_num_t)(dev >> 8);
}

// Master implementation
bool i2c_master_init(vl53l1x_i2c_handle_t *i2c_handle)
{
    if (i2c_handle->scl_gpio == GPIO_NUM_NC) {
        i2c_handle->scl_gpio = I2C_SCL_IO;
    }
    if (i2c_handle->sda_gpio == GPIO_NUM_NC) {
        i2c_handle->sda_gpio = I2C_SDA_IO;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_handle->i2c_port,
        .scl_io_num = i2c_handle->scl_gpio,
        .sda_io_num = i2c_handle->sda_gpio,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = 0}};

    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_handle->bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to initialize i2c: %s", esp_err_to_name(err));
        return false;
    }

    i2c_handle->initialized = true;
    return true;
}

bool i2c_add_device(vl53l1x_device_handle_t *device)
{
    esp_err_t err = i2c_master_probe(device->vl53l1x_handle->i2c_handle->bus_handle, device->i2c_address, 1000);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Sensor found at address 0x%02X", device->i2c_address);
    }
    else
    {
        ESP_LOGE(TAG, "Sensor not responding at 0x%02X: %s", device->i2c_address, esp_err_to_name(err));
        ESP_LOGW(TAG, "found device at 0x%02X instead", i2c_find_first_addr(device->vl53l1x_handle->i2c_handle));
        return false;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device->i2c_address,
        .scl_speed_hz = device->scl_speed_hz};

    dev_handle_t dev_handle;
    err = i2c_master_bus_add_device(device->vl53l1x_handle->i2c_handle->bus_handle, &dev_config, &dev_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to create i2c device: %s", esp_err_to_name(err));
        return false;
    }

    uint16_t dev = create_dev(device->vl53l1x_handle->i2c_handle->i2c_port, device->i2c_address);
    add_device_key(dev, dev_handle);
    device->dev = dev;
    device->dev_handle = dev_handle;
    return true;
}

bool i2c_update_address(const uint16_t dev, const uint8_t new_address)
{
    if (VL53L1X_SetI2CAddress(dev, new_address) != 0)
    {
        ESP_LOGE(TAG, "VL53L1X device failed to update address");
        return false;
    }

    VL53L1_WaitMs(dev, 5);

    dev_handle_t dev_handle = get_device_handle(dev);
    if (i2c_master_device_change_address(dev_handle, new_address, 1000) != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c bus failed to update device address");
        VL53L1X_SetI2CAddress(new_address, get_address(dev));
        return false;
    }

    change_device_address(dev, new_address);
    return true;
}

uint8_t i2c_find_first_addr(vl53l1x_i2c_handle_t *i2c_handle)
{
    for (uint8_t address = 1; address < 0x78; address++)
    {
        if (i2c_master_probe(i2c_handle->bus_handle, address, 1000) == ESP_OK)
        {
            return address;
        }
    }
    return 0;
}

bool i2c_write(const dev_handle_t *device, uint16_t reg, uint8_t value)
{
    uint8_t data[3] =
        {
            reg >> 8,
            reg & 0xFF,
            value};
    if (i2c_master_transmit(*device, data, 3, 1000) != ESP_OK)
    {
        ESP_LOGE(TAG, "write failed at: %04X - %02X", reg, value);
        return false;
    }
    return true;
}

bool i2c_read(const dev_handle_t *device, uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t reg_bytes[2] = {
        reg >> 8,
        reg & 0xFF};

    if (i2c_master_transmit_receive(*device, reg_bytes, 2, buf, len, 1000) != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_read failed on reg 0x%04X", reg);
        return false;
    }
    return true;
}

int8_t i2c_write_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    dev_handle_t handle = get_device_handle(dev);
    if (handle == NULL) return 255;

    for (uint32_t i = 0; i < count; i++)
    {
        if (!i2c_write(&handle, index + i, pdata[i]))
        {
            ESP_LOGE(TAG, "I2C write byte failed at index 0x%X", (unsigned int)(index + i));
            return 255;
        }
    }
    return 0;
}

int8_t i2c_read_multi(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    dev_handle_t handle = get_device_handle(dev);
    if (handle == NULL) return 255;

    if (!i2c_read(&handle, index, pdata, count))
    {
        return 255;
    }
    return 0;
}

int8_t i2c_write_byte(uint16_t dev, uint16_t index, uint8_t data)
{
    return i2c_write_multi(dev, index, &data, 1);
}

int8_t i2c_write_word(uint16_t dev, uint16_t index, uint16_t data)
{
    uint8_t buffer[2];
    buffer[0] = (data >> 8) & 0xFF; // MSB
    buffer[1] = data & 0xFF;        // LSB
    return i2c_write_multi(dev, index, buffer, 2);
}

int8_t i2c_write_dword(uint16_t dev, uint16_t index, uint32_t data)
{
    uint8_t buffer[4];
    buffer[0] = (data >> 24) & 0xFF;
    buffer[1] = (data >> 16) & 0xFF;
    buffer[2] = (data >> 8) & 0xFF;
    buffer[3] = data & 0xFF;
    return i2c_write_multi(dev, index, buffer, 4);
}

int8_t i2c_read_byte(uint16_t dev, uint16_t index, uint8_t *pdata)
{
    return i2c_read_multi(dev, index, pdata, 1);
}

int8_t i2c_read_word(uint16_t dev, uint16_t index, uint16_t *pdata)
{
    uint8_t buffer[2];
    int8_t status = i2c_read_multi(dev, index, buffer, 2);
    if (status == 0)
    {
        *pdata = ((uint16_t)buffer[0] << 8) | buffer[1];
    }
    return status;
}

int8_t i2c_read_dword(uint16_t dev, uint16_t index, uint32_t *pdata)
{
    uint8_t buffer[4];
    int8_t status = i2c_read_multi(dev, index, buffer, 4);
    if (status == 0)
    {
        *pdata = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
                 ((uint32_t)buffer[2] << 8) | buffer[3];
    }
    return status;
}
