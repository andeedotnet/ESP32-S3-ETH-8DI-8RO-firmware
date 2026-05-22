#include "nvs_config/nvs_config.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG       = "nvs_cfg";
static const char *NAMESPACE = "app_cfg";

esp_err_t nvs_config_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    return ESP_OK;
}

esp_err_t nvs_config_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    ssid[0] = '\0';
    pass[0] = '\0';
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    esp_err_t r1 = nvs_get_str(h, "wifi_ssid", ssid, &ssid_len);
    esp_err_t r2 = nvs_get_str(h, "wifi_pass", pass, &pass_len);
    nvs_close(h);
    return (r1 == ESP_OK && r2 == ESP_OK) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_config_set_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, "wifi_ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, "wifi_pass", pass));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi credentials saved (ssid=%s)", ssid);
    return ESP_OK;
}

esp_err_t nvs_config_get_webhook(int input_idx, char *url, size_t url_len, uint8_t *trigger)
{
    char url_key[20], trig_key[20];
    snprintf(url_key,  sizeof(url_key),  "wh%d_url",  input_idx + 1);
    snprintf(trig_key, sizeof(trig_key), "wh%d_trig", input_idx + 1);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        url[0]   = '\0';
        *trigger = NVS_WEBHOOK_TRIG_RISING;
        return ret;
    }
    esp_err_t r1 = nvs_get_str(h, url_key, url, &url_len);
    if (r1 != ESP_OK) url[0] = '\0';
    esp_err_t r2 = nvs_get_u8(h, trig_key, trigger);
    if (r2 != ESP_OK) *trigger = NVS_WEBHOOK_TRIG_RISING;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_set_webhook(int input_idx, const char *url, uint8_t trigger)
{
    char url_key[20], trig_key[20];
    snprintf(url_key,  sizeof(url_key),  "wh%d_url",  input_idx + 1);
    snprintf(trig_key, sizeof(trig_key), "wh%d_trig", input_idx + 1);

    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, url_key, url));
    ESP_ERROR_CHECK(nvs_set_u8(h, trig_key, trigger));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    return ESP_OK;
}

static const char * const s_whe_keys[] = {
    "di1_whe", "di2_whe", "di3_whe", "di4_whe",
    "di5_whe", "di6_whe", "di7_whe", "di8_whe",
};

esp_err_t nvs_config_get_webhook_enabled(int input_idx, uint8_t *enabled)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        *enabled = 1;
        return ret;
    }
    ret = nvs_get_u8(h, s_whe_keys[input_idx], enabled);
    if (ret != ESP_OK) *enabled = 1; /* default: enabled */
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_set_webhook_enabled(int input_idx, uint8_t enabled)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_whe_keys[input_idx], enabled));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    return ESP_OK;
}

static const char * const s_mode_keys[] = {
    "di1_mode", "di2_mode", "di3_mode", "di4_mode",
    "di5_mode", "di6_mode", "di7_mode", "di8_mode",
};

esp_err_t nvs_config_get_input_mode(int input_idx, uint8_t *mode)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        *mode = NVS_INPUT_MODE_OFF;
        return ret;
    }
    ret = nvs_get_u8(h, s_mode_keys[input_idx], mode);
    if (ret != ESP_OK) *mode = NVS_INPUT_MODE_OFF;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_set_input_mode(int input_idx, uint8_t mode)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_mode_keys[input_idx], mode));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    return ESP_OK;
}

static const char * const s_rly_keys[] = {
    "di1_rly", "di2_rly", "di3_rly", "di4_rly",
    "di5_rly", "di6_rly", "di7_rly", "di8_rly",
};

esp_err_t nvs_config_get_relay_target(int input_idx, uint8_t *relay_num)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        *relay_num = (uint8_t)(1 << input_idx);
        return ret;
    }
    ret = nvs_get_u8(h, s_rly_keys[input_idx], relay_num);
    if (ret != ESP_OK) *relay_num = (uint8_t)(1 << input_idx); /* default: same-index relay as bitmask */
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_set_relay_target(int input_idx, uint8_t relay_num)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_rly_keys[input_idx], relay_num));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_get_ntp(char *server, size_t server_len, char *tz, size_t tz_len)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NAMESPACE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        strncpy(server, "pool.ntp.org", server_len - 1);
        strncpy(tz,     "UTC0",         tz_len - 1);
        return ret;
    }
    esp_err_t r1 = nvs_get_str(h, "ntp_server", server, &server_len);
    if (r1 != ESP_OK) strncpy(server, "pool.ntp.org", server_len - 1);
    esp_err_t r2 = nvs_get_str(h, "ntp_tz", tz, &tz_len);
    if (r2 != ESP_OK) strncpy(tz, "UTC0", tz_len - 1);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t nvs_config_set_ntp(const char *server, const char *tz)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, "ntp_server", server));
    ESP_ERROR_CHECK(nvs_set_str(h, "ntp_tz",     tz));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);
    ESP_LOGI("nvs_cfg", "NTP config saved: server=%s tz=%s", server, tz);
    return ESP_OK;
}
