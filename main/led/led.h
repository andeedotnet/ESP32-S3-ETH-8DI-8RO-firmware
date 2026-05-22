#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    LED_OFF      = 0,
    LED_BOOTING,        /* orange blink — waiting for network */
    LED_CONNECTED,      /* solid blue — network connected */
    LED_NO_NETWORK,     /* fast red blink */
} led_pattern_t;

esp_err_t led_init(void);
void      led_set_color(uint8_t r, uint8_t g, uint8_t b);
void      led_set_pattern(led_pattern_t pattern);
