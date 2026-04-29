/**
 * @file spi.h
 * @brief SPI bit-bang cho giao tiếp PS2 controller
 *
 * PS2 controller dùng SPI Mode 3 (CPOL=1, CPHA=1), LSB-first.
 * Dùng bit-bang GPIO thay vì SPI hardware để đảm bảo timing chính xác.
 *
 * Sơ đồ chân:
 *   MOSI (CMD)  → IO11
 *   MISO (DAT)  → IO13
 *   SCK  (CLK)  → IO12
 *   CS   (ATT)  → IO14
 *   ACK          → IO21
 */

#ifndef SPI_PS2_H
#define SPI_PS2_H

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Cấu hình chân ---- */
#define SPI_MOSI_IO     GPIO_NUM_11     /* CMD  – ESP32 → Controller */
#define SPI_MISO_IO     GPIO_NUM_13     /* DAT  – Controller → ESP32 */
#define SPI_SCK_IO      GPIO_NUM_12     /* CLK */
#define SPI_CS_IO       GPIO_NUM_14     /* ATT  – Chip Select (active LOW) */
#define SPI_ACK_IO      GPIO_NUM_21     /* ACK */

/**
 * @brief Khởi tạo các GPIO cho SPI bit-bang
 */
esp_err_t spi_ps2_init(void);

/**
 * @brief Kéo CS xuống LOW (bắt đầu giao tiếp)
 */
void spi_cs_low(void);

/**
 * @brief Kéo CS lên HIGH (kết thúc giao tiếp)
 */
void spi_cs_high(void);

/**
 * @brief Truyền/nhận 1 byte (full-duplex, LSB-first, Mode 3)
 *
 * @param tx_byte  Byte gửi đi (CMD)
 * @return uint8_t Byte nhận về (DAT)
 */
uint8_t spi_transfer_byte(uint8_t tx_byte);

/**
 * @brief Truyền/nhận mảng byte
 *
 * @param tx    Mảng byte gửi
 * @param rx    Mảng byte nhận (có thể NULL nếu không cần)
 * @param len   Số byte
 */
void spi_transfer(const uint8_t *tx, uint8_t *rx, int len);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PS2_H */
