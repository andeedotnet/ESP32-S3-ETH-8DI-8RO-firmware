#include "led/led.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "led";

/* WS2812 timing (ns) at 10 MHz RMT resolution */
#define T0H_NS  400
#define T0L_NS  850
#define T1H_NS  800
#define T1L_NS  450
#define RES_NS  55000   /* reset pulse */

#define RMT_RESOLUTION_HZ 10000000UL  /* 10 MHz */

static rmt_channel_handle_t  s_tx_chan  = NULL;
static rmt_encoder_handle_t  s_encoder  = NULL;
static QueueHandle_t          s_pattern_q = NULL;

/* Encode a single GRB pixel as 24 RMT items */
static void encode_pixel(uint8_t r, uint8_t g, uint8_t b,
                          rmt_symbol_word_t *out)
{
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    uint32_t ns  = 1000000000UL / RMT_RESOLUTION_HZ;
    for (int i = 23; i >= 0; i--) {
        if ((grb >> i) & 1) {
            out[23 - i].duration0 = T1H_NS / ns;
            out[23 - i].level0    = 1;
            out[23 - i].duration1 = T1L_NS / ns;
            out[23 - i].level1    = 0;
        } else {
            out[23 - i].duration0 = T0H_NS / ns;
            out[23 - i].level0    = 1;
            out[23 - i].duration1 = T0L_NS / ns;
            out[23 - i].level1    = 0;
        }
    }
}

void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_tx_chan) return;

    rmt_symbol_word_t symbols[24];
    encode_pixel(r, g, b, symbols);

    rmt_transmit_config_t tx_cfg = {.loop_count = 0};
    rmt_transmit(s_tx_chan, s_encoder, symbols, sizeof(symbols), &tx_cfg);
    rmt_tx_wait_all_done(s_tx_chan, pdMS_TO_TICKS(20));
}

static void led_task(void *arg)
{
    led_pattern_t pattern = LED_BOOTING;
    while (1) {
        /* Check for pattern update without blocking */
        xQueueReceive(s_pattern_q, &pattern, 0);

        switch (pattern) {
        case LED_BOOTING:
            led_set_color(30, 10, 0);   /* orange */
            vTaskDelay(pdMS_TO_TICKS(500));
            led_set_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        case LED_CONNECTED:
            led_set_color(0, 0, 30);    /* solid blue */
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        case LED_NO_NETWORK:
            led_set_color(30, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
            led_set_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(150));
            break;
        default:
            led_set_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
    }
}

esp_err_t led_init(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num            = BOARD_RGB_LED_GPIO,
        .clk_src             = RMT_CLK_SRC_DEFAULT,
        .resolution_hz       = RMT_RESOLUTION_HZ,
        .mem_block_symbols   = 64,
        .trans_queue_depth   = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_tx_chan));

    /* Use the simple copy encoder — we supply pre-encoded RMT symbols */
    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc_cfg, &s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_tx_chan));

    s_pattern_q = xQueueCreate(1, sizeof(led_pattern_t));
    configASSERT(s_pattern_q != NULL);
    BaseType_t ret = xTaskCreate(led_task, "led_task", 3072, NULL, 1, NULL);
    configASSERT(ret == pdPASS);
    ESP_LOGI(TAG, "WS2812 LED initialized on GPIO%d", BOARD_RGB_LED_GPIO);
    return ESP_OK;
}

void led_set_pattern(led_pattern_t pattern)
{
    if (s_pattern_q) {
        xQueueOverwrite(s_pattern_q, &pattern);
    }
}
