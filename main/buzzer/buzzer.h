#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t buzzer_init(void);
void      buzzer_beep(uint32_t duration_ms);
