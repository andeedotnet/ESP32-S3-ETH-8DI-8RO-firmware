#include "digital_input/digital_input.h"
#include "app_state.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "di";

QueueHandle_t g_di_change_queue = NULL;
static QueueHandle_t s_gpio_evt_queue = NULL;

static void IRAM_ATTR di_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    xQueueSendFromISR(s_gpio_evt_queue, &gpio_num, NULL);
}

static void di_task(void *arg)
{
    /* Subscribe to the (panic-enabled) WDT so a wedged input task reboots. */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    uint32_t gpio_num;
    while (1) {
        /* Wake at least once per second to feed the dog even with no edges. */
        if (xQueueReceive(s_gpio_evt_queue, &gpio_num, pdMS_TO_TICKS(1000)) != pdTRUE) {
            esp_task_wdt_reset();
            continue;
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(20)); /* debounce */

        int idx = (int)gpio_num - BOARD_DI_GPIO_BASE;
        if (idx < 0 || idx >= BOARD_DI_COUNT) {
            continue; /* spurious ISR from unexpected GPIO */
        }
        bool level = gpio_get_level(gpio_num) == 1;

        bool changed = false;
        if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (g_state.di_states[idx] != level) {
                g_state.di_states[idx] = level;
                changed = true;
            }
            xSemaphoreGive(g_state_mutex);
        }

        if (changed) {
            ESP_LOGI(TAG, "DI%d -> %s", idx + 1, level ? "HIGH" : "LOW");
            di_event_t evt = {.input_idx = (uint8_t)idx, .level = level};
            xQueueSend(g_di_change_queue, &evt, 0);
        }
    }
}

esp_err_t di_init(void)
{
    s_gpio_evt_queue  = xQueueCreate(16, sizeof(uint32_t));
    g_di_change_queue = xQueueCreate(16, sizeof(di_event_t));
    configASSERT(s_gpio_evt_queue  != NULL);
    configASSERT(g_di_change_queue != NULL);

    gpio_config_t io_cfg = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
        .pin_bit_mask = 0,
    };
    for (int i = 0; i < BOARD_DI_COUNT; i++) {
        io_cfg.pin_bit_mask |= (1ULL << (BOARD_DI_GPIO_BASE + i));
    }
    ESP_ERROR_CHECK(gpio_config(&io_cfg));

    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        return isr_ret;
    }
    for (int i = 0; i < BOARD_DI_COUNT; i++) {
        gpio_num_t pin = BOARD_DI_GPIO_BASE + i;
        gpio_isr_handler_add(pin, di_isr_handler, (void *)(uintptr_t)pin);
    }

    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < BOARD_DI_COUNT; i++) {
            g_state.di_states[i] = gpio_get_level(BOARD_DI_GPIO_BASE + i) == 1;
        }
        xSemaphoreGive(g_state_mutex);
    }

    BaseType_t ret = xTaskCreate(di_task, "di_task", 3072, NULL, 5, NULL);
    configASSERT(ret == pdPASS);

    ESP_LOGI(TAG, "Digital inputs initialized (GPIO%d..GPIO%d)",
             BOARD_DI_GPIO_BASE, BOARD_DI_GPIO_BASE + BOARD_DI_COUNT - 1);
    return ESP_OK;
}

void di_get_all(bool states[8])
{
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(states, g_state.di_states, sizeof(g_state.di_states));
        xSemaphoreGive(g_state_mutex);
    }
}
