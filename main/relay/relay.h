#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t relay_init(void);
esp_err_t relay_set(uint8_t relay_num, bool state);   /* relay_num: 1-based */
uint8_t   relay_get_mask(void);                        /* bit 0 = relay 1 */
