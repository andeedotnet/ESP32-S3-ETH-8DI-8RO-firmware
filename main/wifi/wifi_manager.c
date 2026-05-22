#include "wifi/wifi_manager.h"
#include "nvs_config/nvs_config.h"
#include "app_state.h"
#include "led/led.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "wifi";

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
            /* Always retry — no give-up limit for a 24/7 device */
            ESP_LOGI(TAG, "WiFi disconnected, retrying...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
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
    esp_wifi_disconnect();

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid,    ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    return ESP_OK;
}
