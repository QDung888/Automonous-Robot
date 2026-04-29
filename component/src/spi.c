/**
 * @file spi.c
 * @brief SPI bit-bang cho PS2 controller – Implementation
 *
 * SPI Mode 3: CLK idle HIGH, data sampled on rising edge.
 * LSB-first. Clock ~125kHz (4µs half-period).
 */

#include "spi.h"
#include "esp_log.h"
#include "rom/ets_sys.h"   /* ets_delay_us() */

static const char *TAG = "SPI_PS2";

/* Thời gian nửa chu kỳ clock (µs) – ~125kHz */
#define SPI_CLK_HALF_US     4
/* Delay giữa các byte (µs) */
#define SPI_BYTE_DELAY_US   16

esp_err_t spi_ps2_init(void)
{
    /* MOSI (CMD) – output */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << SPI_MOSI_IO) | (1ULL << SPI_SCK_IO) | (1ULL << SPI_CS_IO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    /* MISO (DAT) – input với pull-up */
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << SPI_MISO_IO) | (1ULL << SPI_ACK_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    /* Trạng thái ban đầu: CLK=HIGH (Mode 3), CS=HIGH (inactive) */
    gpio_set_level(SPI_SCK_IO, 1);
    gpio_set_level(SPI_CS_IO, 1);
    gpio_set_level(SPI_MOSI_IO, 1);

    ESP_LOGI(TAG, "SPI PS2 init OK (MOSI=IO%d, MISO=IO%d, SCK=IO%d, CS=IO%d, ACK=IO%d)",
             SPI_MOSI_IO, SPI_MISO_IO, SPI_SCK_IO, SPI_CS_IO, SPI_ACK_IO);
    return ESP_OK;
}

void spi_cs_low(void)
{
    gpio_set_level(SPI_CS_IO, 0);
    ets_delay_us(SPI_BYTE_DELAY_US);
}

void spi_cs_high(void)
{
    gpio_set_level(SPI_CS_IO, 1);
    ets_delay_us(SPI_BYTE_DELAY_US);
}

uint8_t spi_transfer_byte(uint8_t tx_byte)
{
    uint8_t rx_byte = 0;

    /*
     * SPI Mode 3, LSB-first:
     *   - CLK idle HIGH
     *   - Đặt data trên cạnh xuống (falling edge)
     *   - Đọc data trên cạnh lên (rising edge)
     */
    for (int i = 0; i < 8; i++) {
        /* Cạnh xuống: đặt bit gửi (LSB first) */
        gpio_set_level(SPI_SCK_IO, 0);
        gpio_set_level(SPI_MOSI_IO, (tx_byte >> i) & 0x01);
        ets_delay_us(SPI_CLK_HALF_US);

        /* Cạnh lên: đọc bit nhận */
        gpio_set_level(SPI_SCK_IO, 1);
        if (gpio_get_level(SPI_MISO_IO)) {
            rx_byte |= (1 << i);
        }
        ets_delay_us(SPI_CLK_HALF_US);
    }

    /* Delay giữa các byte */
    ets_delay_us(SPI_BYTE_DELAY_US);

    return rx_byte;
}

void spi_transfer(const uint8_t *tx, uint8_t *rx, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t result = spi_transfer_byte(tx ? tx[i] : 0x00);
        if (rx) {
            rx[i] = result;
        }
    }
}
