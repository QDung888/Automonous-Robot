/**
 * @file TB6612FNG.c
 * @brief Driver 2x TB6612FNG + 4 Encoder – Implementation
 */

#include "TB6612FNG.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TB6612";

/* ==================== Encoder (PCNT) ==================== */

static const char *enc_names[ENCODER_COUNT] = {"MotA_L", "MotB_L", "MotA_R", "MotB_R"};

/** Khởi tạo 1 encoder quadrature bằng PCNT */
static esp_err_t encoder_init_one(pcnt_unit_handle_t *unit, int gpio_a, int gpio_b, const char *name)
{
    pcnt_unit_config_t unit_cfg = {
        .high_limit = TB6612_PCNT_HIGH_LIMIT,
        .low_limit = TB6612_PCNT_LOW_LIMIT,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, unit), TAG, "PCNT unit %s", name);

    /* Bộ lọc glitch 1µs */
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 1000,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(*unit, &filter_cfg), TAG, "filter %s", name);

    /* Kênh A: đếm cạnh trên gpio_a, tham chiếu gpio_b */
    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = gpio_a,
        .level_gpio_num = gpio_b,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(*unit, &chan_a_cfg, &chan_a), TAG, "chanA %s", name);

    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE), TAG, "edgeA %s", name);
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "lvlA %s", name);

    /* Kênh B: đếm cạnh trên gpio_b, tham chiếu gpio_a */
    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = gpio_b,
        .level_gpio_num = gpio_a,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(*unit, &chan_b_cfg, &chan_b), TAG, "chanB %s", name);

    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE), TAG, "edgeB %s", name);
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "lvlB %s", name);

    /* Enable và start */
    ESP_RETURN_ON_ERROR(pcnt_unit_enable(*unit), TAG, "enable %s", name);
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(*unit), TAG, "clear %s", name);
    ESP_RETURN_ON_ERROR(pcnt_unit_start(*unit), TAG, "start %s", name);

    ESP_LOGI(TAG, "Encoder %s OK (A=IO%d, B=IO%d)", name, gpio_a, gpio_b);
    return ESP_OK;
}

/* ==================== PWM (LEDC) ==================== */

static void pwm_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = TB6612_PWM_RESOLUTION,
        .freq_hz = TB6612_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    /* 4 kênh PWM */
    const struct { int gpio; ledc_channel_t ch; const char *name; } channels[] = {
        { TB6612_PWMA_R_GPIO, LEDC_CHANNEL_0, "PWMA_R" },
        { TB6612_PWMB_R_GPIO, LEDC_CHANNEL_1, "PWMB_R" },
        { TB6612_PWMA_L_GPIO, LEDC_CHANNEL_2, "PWMA_L" },
        { TB6612_PWMB_L_GPIO, LEDC_CHANNEL_3, "PWMB_L" },
    };

    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = channels[i].ch,
            .timer_sel = LEDC_TIMER_0,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = channels[i].gpio,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
        ESP_LOGI(TAG, "PWM %s (IO%d) → CH%d OK", channels[i].name, channels[i].gpio, i);
    }
}

/* ==================== STBY GPIO ==================== */

static void stby_init(void)
{
    gpio_config_t stby_cfg = {
        .pin_bit_mask = (1ULL << TB6612_STBY_R_GPIO) | (1ULL << TB6612_STBY_L_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&stby_cfg));
    ESP_ERROR_CHECK(gpio_set_level(TB6612_STBY_R_GPIO, 1));
    ESP_ERROR_CHECK(gpio_set_level(TB6612_STBY_L_GPIO, 1));
    ESP_LOGI(TAG, "STBY_R=HIGH (IO%d), STBY_L=HIGH (IO%d)",
             TB6612_STBY_R_GPIO, TB6612_STBY_L_GPIO);
}

/* ==================== Encoder print task ==================== */

static tb6612_system_t *s_print_sys = NULL;

static void encoder_print_task(void *arg)
{
    int counts[ENCODER_COUNT];
    while (1) {
        for (int i = 0; i < ENCODER_COUNT; i++) {
            pcnt_unit_get_count(s_print_sys->enc_units[i], &counts[i]);
        }

        /* Tính số vòng trục ra (có dấu: + = thuận, - = nghịch) */
        float rev_al = (float)counts[0] / TB6612_TICKS_PER_REV;
        float rev_bl = (float)counts[1] / TB6612_TICKS_PER_REV;
        float rev_ar = (float)counts[2] / TB6612_TICKS_PER_REV;
        float rev_br = (float)counts[3] / TB6612_TICKS_PER_REV;

        ESP_LOGI("ENCODER",
                 "AL:%+6d (%+.2f rev)  BL:%+6d (%+.2f rev)  |  AR:%+6d (%+.2f rev)  BR:%+6d (%+.2f rev)",
                 counts[0], rev_al, counts[1], rev_bl,
                 counts[2], rev_ar, counts[3], rev_br);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ==================== Public API ==================== */

esp_err_t tb6612_init(tb6612_system_t *sys, pcf8575_handle_t *pcf_handle)
{
    if (sys == NULL || pcf_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sys->pcf_handle = pcf_handle;

    /* 1. STBY */
    stby_init();

    /* 2. PWM */
    pwm_init();

    /* 3. Encoder – 4 bộ PCNT quadrature */
    ESP_RETURN_ON_ERROR(encoder_init_one(&sys->enc_units[ENCODER_MOTOR_AL],
        TB6612_ENC_AL_A, TB6612_ENC_AL_B, enc_names[ENCODER_MOTOR_AL]), TAG, "enc AL");
    ESP_RETURN_ON_ERROR(encoder_init_one(&sys->enc_units[ENCODER_MOTOR_BL],
        TB6612_ENC_BL_A, TB6612_ENC_BL_B, enc_names[ENCODER_MOTOR_BL]), TAG, "enc BL");
    ESP_RETURN_ON_ERROR(encoder_init_one(&sys->enc_units[ENCODER_MOTOR_AR],
        TB6612_ENC_AR_A, TB6612_ENC_AR_B, enc_names[ENCODER_MOTOR_AR]), TAG, "enc AR");
    ESP_RETURN_ON_ERROR(encoder_init_one(&sys->enc_units[ENCODER_MOTOR_BR],
        TB6612_ENC_BR_A, TB6612_ENC_BR_B, enc_names[ENCODER_MOTOR_BR]), TAG, "enc BR");

    sys->initialized = true;
    ESP_LOGI(TAG, "TB6612 system khởi tạo hoàn tất (2x TB6612 + 4 encoder)");
    return ESP_OK;
}

esp_err_t tb6612_set_direction(tb6612_system_t *sys, uint8_t direction)
{
    if (sys == NULL || sys->pcf_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Ghi byte thấp (P0-P7) chứa cả RIGHT + LEFT */
    return pcf8575_write(sys->pcf_handle, (uint16_t)direction);
}

void tb6612_set_speed_all(uint8_t duty)
{
    for (int ch = 0; ch < 4; ch++) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)ch, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)ch);
    }
    ESP_LOGI(TAG, "PWM 4 kênh → duty=%d/255", duty);
}

esp_err_t tb6612_stop_all(tb6612_system_t *sys)
{
    return tb6612_set_direction(sys, CAR_STOP);
}

esp_err_t tb6612_encoder_get_count(tb6612_system_t *sys, encoder_id_t id, int *count)
{
    if (sys == NULL || id >= ENCODER_COUNT || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return pcnt_unit_get_count(sys->enc_units[id], count);
}

esp_err_t tb6612_encoder_clear_all(tb6612_system_t *sys)
{
    if (sys == NULL) return ESP_ERR_INVALID_ARG;
    for (int i = 0; i < ENCODER_COUNT; i++) {
        pcnt_unit_clear_count(sys->enc_units[i]);
    }
    return ESP_OK;
}

void tb6612_encoder_start_print_task(tb6612_system_t *sys)
{
    s_print_sys = sys;
    xTaskCreate(encoder_print_task, "enc_print", 2048, NULL, 5, NULL);
}
