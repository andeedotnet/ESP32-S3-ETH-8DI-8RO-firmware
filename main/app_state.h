#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "board_config.h"

typedef struct {
    uint8_t relay_mask;                     /* bit 0 = relay 1, bit 7 = relay 8 */
    bool    di_states[BOARD_DI_COUNT];      /* true = active/energized */
    bool    eth_connected;
    bool    wifi_connected;
    char    eth_ip[16];
    char    wifi_ip[16];
} app_runtime_state_t;

extern app_runtime_state_t g_state;
extern SemaphoreHandle_t   g_state_mutex;

void app_state_init(void);
