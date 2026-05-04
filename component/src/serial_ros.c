/**
 * @file serial_ros.c
 * @brief Truyền nhận dữ liệu encoder qua UART0 (CP2102) – Implementation
 *
 * Thiết kế ổn định:
 *   - Tất cả task pinned to Core 1 (tách biệt với motor/joystick trên Core 0)
 *   - TX task priority 7 (cao) để đảm bảo gửi data không bị ngắt quãng
 *   - UART event queue để xử lý RX chính xác
 *   - Buffer 1024 byte (theo rosserial_esp32)
 *
 * TX task: đọc 4 encoder PCNT → gửi "$ENC,<AL>,<BL>,<AR>,<BR>\r\n" mỗi 50ms
 * RX task: parse "$CMD,<dir>,<speed>\r\n" từ Raspberry Pi
 */

#include "serial_ros.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SERIAL_ROS";

static tb6612_system_t     *s_motor_sys = NULL;
static serial_ros_cmd_cb_t  s_cmd_cb    = NULL;
static QueueHandle_t        s_uart_queue = NULL;

/* ==================== UART Init ==================== */

esp_err_t serial_ros_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = SERIAL_ROS_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(
        uart_param_config(SERIAL_ROS_UART_NUM, &uart_cfg),
        TAG, "uart_param_config failed");

    ESP_RETURN_ON_ERROR(
        uart_set_pin(SERIAL_ROS_UART_NUM,
                     SERIAL_ROS_TX_IO, SERIAL_ROS_RX_IO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
        TAG, "uart_set_pin failed");

    /*
     * Install UART driver với event queue (tham khảo rosserial_esp32)
     * RX buffer = 1024, TX buffer = 1024
     * Event queue depth = 20
     */
    ESP_RETURN_ON_ERROR(
        uart_driver_install(SERIAL_ROS_UART_NUM,
                            SERIAL_ROS_BUF_SIZE,    /* RX buffer */
                            SERIAL_ROS_BUF_SIZE,    /* TX buffer */
                            20,                      /* Event queue size */
                            &s_uart_queue,
                            0),
        TAG, "uart_driver_install failed");

    ESP_LOGI(TAG, "UART%d init OK (baud=%d, TX=IO%d, RX=IO%d, Core %d)",
             SERIAL_ROS_UART_NUM, SERIAL_ROS_BAUD_RATE,
             SERIAL_ROS_TX_IO, SERIAL_ROS_RX_IO, SERIAL_ROS_CORE);
    return ESP_OK;
}

/* ==================== Gửi encoder data ==================== */

void serial_ros_send_encoder(const int ticks[4])
{
    char buf[80];
    int len = snprintf(buf, sizeof(buf), "$ENC,%d,%d,%d,%d\r\n",
                       ticks[0], ticks[1], ticks[2], ticks[3]);
    if (len > 0) {
        uart_write_bytes(SERIAL_ROS_UART_NUM, buf, len);
    }
}

/* ==================== Parse lệnh từ Pi ==================== */

/**
 * Parse "$CMD,<dir>,<speed>\r\n"
 * dir: F/B/L/R/S   speed: 0-255
 */
static void parse_command(const char *line)
{
    if (strncmp(line, "$CMD,", 5) != 0) return;

    char dir_char = line[5];
    int speed = 0;

    /* Tìm speed sau dấu phẩy thứ 2 */
    const char *p = strchr(line + 5, ',');
    if (p) {
        speed = atoi(p + 1);
        if (speed < 0) speed = 0;
        if (speed > 255) speed = 255;
    }

    uint8_t direction = CAR_STOP;
    switch (dir_char) {
        case 'F': case 'f': direction = CAR_FORWARD;    break;
        case 'B': case 'b': direction = CAR_BACKWARD;   break;
        case 'L': case 'l': direction = CAR_TURN_LEFT;  break;
        case 'R': case 'r': direction = CAR_TURN_RIGHT; break;
        case 'S': case 's': direction = CAR_STOP;       break;
        default:
            ESP_LOGW(TAG, "Unknown direction: '%c'", dir_char);
            return;
    }

    ESP_LOGI(TAG, "CMD from Pi: dir=%c speed=%d", dir_char, speed);

    if (s_cmd_cb) {
        s_cmd_cb(direction, (uint8_t)speed);
    }
}

/* ==================== TX Task (Core 1, priority 7) ==================== */

static void serial_ros_tx_task(void *arg)
{
    int ticks[4];

    ESP_LOGI(TAG, "TX task started on Core %d (priority %d) – gửi mỗi %dms",
             xPortGetCoreID(), uxTaskPriorityGet(NULL), SERIAL_ROS_TX_INTERVAL_MS);

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        /* Đọc 4 encoder counts */
        for (int i = 0; i < ENCODER_COUNT; i++) {
            tb6612_encoder_get_count(s_motor_sys, (encoder_id_t)i, &ticks[i]);
        }

        /* Gửi qua UART */
        serial_ros_send_encoder(ticks);

        /*
         * Dùng vTaskDelayUntil thay vì vTaskDelay để đảm bảo
         * tần suất gửi chính xác, không bị drift theo thời gian xử lý
         */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SERIAL_ROS_TX_INTERVAL_MS));
    }
}

/* ==================== RX Task (Core 1, priority 6) ==================== */

static void serial_ros_rx_task(void *arg)
{
    uint8_t rx_buf[SERIAL_ROS_BUF_SIZE];
    char line_buf[SERIAL_ROS_BUF_SIZE];
    int line_pos = 0;
    uart_event_t event;

    ESP_LOGI(TAG, "RX task started on Core %d (priority %d) – chờ lệnh từ Pi",
             xPortGetCoreID(), uxTaskPriorityGet(NULL));

    while (1) {
        /* Chờ UART event từ queue */
        if (xQueueReceive(s_uart_queue, &event, pdMS_TO_TICKS(100))) {
            switch (event.type) {
                case UART_DATA: {
                    int len = uart_read_bytes(SERIAL_ROS_UART_NUM, rx_buf,
                                              event.size, pdMS_TO_TICKS(50));
                    if (len <= 0) break;

                    /* Ghép byte vào line buffer, parse khi gặp '\n' */
                    for (int i = 0; i < len; i++) {
                        char c = (char)rx_buf[i];

                        if (c == '\n' || c == '\r') {
                            if (line_pos > 0) {
                                line_buf[line_pos] = '\0';
                                parse_command(line_buf);
                                line_pos = 0;
                            }
                        } else {
                            if (line_pos < (int)sizeof(line_buf) - 1) {
                                line_buf[line_pos++] = c;
                            }
                        }
                    }
                    break;
                }

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO overflow");
                    uart_flush_input(SERIAL_ROS_UART_NUM);
                    xQueueReset(s_uart_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART ring buffer full");
                    uart_flush_input(SERIAL_ROS_UART_NUM);
                    xQueueReset(s_uart_queue);
                    break;

                default:
                    break;
            }
        }
    }
}

/* ==================== Start (pinned to Core 1) ==================== */

void serial_ros_start(tb6612_system_t *motor_sys, serial_ros_cmd_cb_t cmd_cb)
{
    s_motor_sys = motor_sys;
    s_cmd_cb = cmd_cb;

    /* TX task: Core 1, priority cao → gửi encoder data ổn định */
    xTaskCreatePinnedToCore(
        serial_ros_tx_task,             /* Task function */
        "sros_tx",                      /* Name */
        4096,                           /* Stack size */
        NULL,                           /* Param */
        SERIAL_ROS_TX_PRIORITY,         /* Priority 7 */
        NULL,                           /* Handle */
        SERIAL_ROS_CORE                 /* Core 1 */
    );

    /* RX task: Core 1, priority thấp hơn TX → xử lý lệnh từ Pi */
    xTaskCreatePinnedToCore(
        serial_ros_rx_task,             /* Task function */
        "sros_rx",                      /* Name */
        4096,                           /* Stack size */
        NULL,                           /* Param */
        SERIAL_ROS_RX_PRIORITY,         /* Priority 6 */
        NULL,                           /* Handle */
        SERIAL_ROS_CORE                 /* Core 1 */
    );

    ESP_LOGI(TAG, "SerialROS tasks created on Core %d (TX pri=%d, RX pri=%d)",
             SERIAL_ROS_CORE, SERIAL_ROS_TX_PRIORITY, SERIAL_ROS_RX_PRIORITY);
}
