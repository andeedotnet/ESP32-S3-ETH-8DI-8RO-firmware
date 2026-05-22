#pragma once

#include "esp_err.h"

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *pass);
