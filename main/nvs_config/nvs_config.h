#pragma once

#include <stdint.h>
#include "esp_err.h"

#define NVS_WEBHOOK_TRIG_RISING  0
#define NVS_WEBHOOK_TRIG_FALLING 1
#define NVS_WEBHOOK_TRIG_CHANGE  2

/* Switch modes — value 0 means no relay switching (webhook-only) */
#define NVS_INPUT_MODE_OFF       0
#define NVS_INPUT_MODE_MOMENTARY 1
#define NVS_INPUT_MODE_LATCHING  2

esp_err_t nvs_config_init(void);

esp_err_t nvs_config_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
esp_err_t nvs_config_set_wifi(const char *ssid, const char *pass);

/* input_idx: 0-based */
esp_err_t nvs_config_get_webhook(int input_idx, char *url, size_t url_len, uint8_t *trigger);
esp_err_t nvs_config_set_webhook(int input_idx, const char *url, uint8_t trigger);

/* Webhook on/off (default 1 = enabled) */
esp_err_t nvs_config_get_webhook_enabled(int input_idx, uint8_t *enabled);
esp_err_t nvs_config_set_webhook_enabled(int input_idx, uint8_t enabled);

/* Switch mode (NVS_INPUT_MODE_*) */
esp_err_t nvs_config_get_input_mode(int input_idx, uint8_t *mode);
esp_err_t nvs_config_set_input_mode(int input_idx, uint8_t mode);

/* Target relay for switch mode (1-based, default = input_idx+1) */
esp_err_t nvs_config_get_relay_target(int input_idx, uint8_t *relay_num);
esp_err_t nvs_config_set_relay_target(int input_idx, uint8_t relay_num);

/* NTP configuration */
esp_err_t nvs_config_get_ntp(char *server, size_t server_len, char *tz, size_t tz_len);
esp_err_t nvs_config_set_ntp(const char *server, const char *tz);
