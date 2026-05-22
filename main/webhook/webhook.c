#include "webhook/webhook.h"
#include "digital_input/digital_input.h"
#include "nvs_config/nvs_config.h"
#include "relay/relay.h"
#include "app_state.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "webhook";

static bool network_is_up(void)
{
    bool up = false;
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        up = g_state.eth_connected || g_state.wifi_connected;
        xSemaphoreGive(g_state_mutex);
    }
    return up;
}

static const char *trigger_name(uint8_t trig)
{
    switch (trig) {
    case NVS_WEBHOOK_TRIG_RISING:  return "rising";
    case NVS_WEBHOOK_TRIG_FALLING: return "falling";
    default:                        return "change";
    }
}

static void webhook_post(const char *url, int input_idx, bool level, uint8_t trig)
{
    if (!network_is_up()) {
        ESP_LOGD(TAG, "DI%d: no network, skipping webhook", input_idx + 1);
        return;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"input\":%d,\"state\":%d,\"trigger\":\"%s\",\"timestamp_ms\":%" PRId64 "}",
             input_idx + 1, level ? 1 : 0, trigger_name(trig),
             (int64_t)(esp_timer_get_time() / 1000));

    esp_http_client_config_t cfg = {
        .url                   = url,
        .method                = HTTP_METHOD_POST,
        .timeout_ms            = 3000,
        .disable_auto_redirect = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "DI%d: http_client_init failed", input_idx + 1);
        return;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, (int)strlen(payload));

    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Webhook DI%d sent (status=%d)", input_idx + 1,
                 esp_http_client_get_status_code(client));
    } else {
        ESP_LOGW(TAG, "Webhook DI%d failed: %s", input_idx + 1, esp_err_to_name(ret));
    }
    esp_http_client_cleanup(client);
}

static void dispatch(int input_idx, bool level)
{
    uint8_t mode = NVS_INPUT_MODE_OFF;
    nvs_config_get_input_mode(input_idx, &mode);

    if (mode == NVS_INPUT_MODE_MOMENTARY && level) {
        uint8_t rmask = (uint8_t)(1 << input_idx);
        nvs_config_get_relay_target(input_idx, &rmask);
        uint8_t current_mask = relay_get_mask();
        for (int r = 0; r < BOARD_RELAY_COUNT; r++) {
            if (rmask & (1 << r)) {
                relay_set(r + 1, !((current_mask >> r) & 1));
            }
        }
        ESP_LOGI(TAG, "DI%d momentary: relay_mask=0x%02x toggled", input_idx + 1, rmask);
    } else if (mode == NVS_INPUT_MODE_LATCHING) {
        uint8_t rmask = (uint8_t)(1 << input_idx);
        nvs_config_get_relay_target(input_idx, &rmask);
        for (int r = 0; r < BOARD_RELAY_COUNT; r++) {
            if (rmask & (1 << r)) {
                relay_set(r + 1, !level);
            }
        }
        ESP_LOGI(TAG, "DI%d latching: relay_mask=0x%02x -> %d", input_idx + 1, rmask, !level);
    }

    uint8_t wh_enabled = 1;
    nvs_config_get_webhook_enabled(input_idx, &wh_enabled);
    if (!wh_enabled) return;

    char    url[256] = {0};
    uint8_t trig     = NVS_WEBHOOK_TRIG_RISING;
    nvs_config_get_webhook(input_idx, url, sizeof(url), &trig);
    if (url[0] == '\0') return;

    bool fire = false;
    switch (trig) {
    case NVS_WEBHOOK_TRIG_RISING:  fire = level;  break;
    case NVS_WEBHOOK_TRIG_FALLING: fire = !level; break;
    default:                        fire = true;   break;
    }
    if (fire) webhook_post(url, input_idx, level, trig);
}

static void webhook_task(void *arg)
{
    di_event_t evt;
    while (1) {
        if (xQueueReceive(g_di_change_queue, &evt, portMAX_DELAY) == pdTRUE) {
            dispatch(evt.input_idx, evt.level);
        }
    }
}

void webhook_apply_boot_states(void)
{
    bool states[BOARD_DI_COUNT];
    di_get_all(states);
    for (int i = 0; i < BOARD_DI_COUNT; i++) {
        uint8_t mode = NVS_INPUT_MODE_OFF;
        nvs_config_get_input_mode(i, &mode);
        if (mode == NVS_INPUT_MODE_LATCHING) {
            uint8_t rmask = (uint8_t)(1 << i);
            nvs_config_get_relay_target(i, &rmask);
            for (int r = 0; r < BOARD_RELAY_COUNT; r++) {
                if (rmask & (1 << r)) relay_set(r + 1, !states[i]);
            }
            ESP_LOGI(TAG, "Boot latching: DI%d relay_mask=0x%02x -> %d", i + 1, rmask, !states[i]);
        }
    }
}

esp_err_t webhook_task_start(void)
{
    BaseType_t ret = xTaskCreate(webhook_task, "webhook_task", 8192, NULL, 3, NULL);
    configASSERT(ret == pdPASS);
    ESP_LOGI(TAG, "Webhook task started");
    return ESP_OK;
}
