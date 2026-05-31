#include "health/health.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "health";

/* How often the task wakes, feeds the watchdog, and re-checks heap. */
#define HEALTH_TICK_MS        2000
/* Log a heap summary roughly once a minute (every Nth tick). */
#define HEALTH_LOG_EVERY      30
/*
 * Critical free-heap floor. Below this the device cannot reliably serve HTTP or
 * keep TCP/IP buffers, so a clean reboot recovers faster than limping along.
 * ~24 KB leaves margin above the point where esp_http_* allocations fail.
 */
#define HEALTH_HEAP_FLOOR     (24 * 1024)

static void health_task(void *arg)
{
    /* Subscribe so a hung scheduler/this task trips the (panic-enabled) WDT. */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    uint32_t tick = 0;
    while (1) {
        esp_task_wdt_reset();

        size_t free_now = esp_get_free_heap_size();
        size_t min_ever = esp_get_minimum_free_heap_size();

        if ((tick % HEALTH_LOG_EVERY) == 0) {
            ESP_LOGI(TAG, "heap free=%u min_ever=%u largest_block=%u",
                     (unsigned)free_now, (unsigned)min_ever,
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
        }

        if (free_now < HEALTH_HEAP_FLOOR) {
            ESP_LOGE(TAG, "Free heap %u below floor %u — rebooting to recover",
                     (unsigned)free_now, (unsigned)HEALTH_HEAP_FLOOR);
            esp_restart();
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(HEALTH_TICK_MS));
    }
}

esp_err_t health_task_start(void)
{
    BaseType_t ret = xTaskCreate(health_task, "health", 3072, NULL, 1, NULL);
    configASSERT(ret == pdPASS);
    ESP_LOGI(TAG, "Health monitor started (floor=%u bytes)", (unsigned)HEALTH_HEAP_FLOOR);
    return ESP_OK;
}
