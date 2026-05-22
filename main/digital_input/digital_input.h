#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    uint8_t input_idx;  /* 0-based index (0 = DI1) */
    bool    level;      /* true = active/high after debounce */
} di_event_t;

extern QueueHandle_t g_di_change_queue;

esp_err_t di_init(void);
void      di_get_all(bool states[8]);
