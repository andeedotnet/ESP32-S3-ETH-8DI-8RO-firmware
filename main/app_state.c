#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

app_runtime_state_t g_state       = {0};
SemaphoreHandle_t   g_state_mutex = NULL;

void app_state_init(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state_mutex = xSemaphoreCreateMutex();
    configASSERT(g_state_mutex != NULL);
}
