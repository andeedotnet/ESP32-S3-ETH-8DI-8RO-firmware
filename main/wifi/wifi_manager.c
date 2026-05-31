#include "wifi/wifi_manager.h"
#include "nvs_config/nvs_config.h"
#include "app_state.h"
#include "led/led.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "wifi";

/* Reconnect backoff: never give up, but don't spin a tight reconnect loop when
 * the AP is down/flapping. Start short, double up to a cap. */
#define WIFI_RECONNECT_MIN_MS   1000
#define WIFI_RECONNECT_MAX_MS   30000

static esp_timer_handle_t s_reconnect_timer = NULL;
static uint32_t           s_backoff_ms      = WIFI_RECONNECT_MIN_MS;

static void reconnect_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "WiFi reconnecting...");
    esp_wifi_connect();
}

static void schedule_reconnect(void)
{
    if (!s_reconnect_timer) {
        esp_wifi_connect();
        return;
    }
    esp_timer_stop(s_reconnect_timer); /* harmless if not running */
    ESP_LOGI(TAG, "WiFi disconnected, retry in %u ms", (unsigned)s_backoff_ms);
    esp_timer_start_once(s_reconnect_timer, (uint64_t)s_backoff_ms * 1000);

    s_backoff_ms *= 2;
    if (s_backoff_ms > WIFI_RECONNECT_MAX_MS) s_backoff_ms = WIFI_RECONNECT_MAX_MS;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_state.wifi_connected = false;
                g_state.wifi_ip[0]    = '\0';
                xSemaphoreGive(g_state_mutex);
            }
            /* Always retry — no give-up limit for a 24/7 device — but back off. */
            schedule_reconnect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        s_backoff_ms = WIFI_RECONNECT_MIN_MS; /* healthy link — reset backoff */
        if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_state.wifi_connected = true;
            snprintf(g_state.wifi_ip, sizeof(g_state.wifi_ip), IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(g_state_mutex);
        }
        ESP_LOGI(TAG, "WiFi IP: %s", g_state.wifi_ip);
        led_set_pattern(LED_CONNECTED);
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_netif_create_default_wifi_sta();

    const esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name     = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    char ssid[33] = {0}, pass[65] = {0};
    if (nvs_config_get_wifi(ssid, sizeof(ssid), pass, sizeof(pass)) == ESP_OK && ssid[0]) {
        wifi_config_t wifi_cfg = {0};
        strlcpy((char *)wifi_cfg.sta.ssid,     ssid, sizeof(wifi_cfg.sta.ssid));
        strlcpy((char *)wifi_cfg.sta.password,  pass, sizeof(wifi_cfg.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_connect());
        ESP_LOGI(TAG, "Connecting to stored WiFi: %s", ssid);
    } else {
        ESP_LOGI(TAG, "No WiFi credentials stored");
    }
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *pass)
{
    s_backoff_ms = WIFI_RECONNECT_MIN_MS; /* user-initiated — try immediately */
    if (s_reconnect_timer) esp_timer_stop(s_reconnect_timer);
    esp_wifi_disconnect();

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid,    ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    return ESP_OK;
}
