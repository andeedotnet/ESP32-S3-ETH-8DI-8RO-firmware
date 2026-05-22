#include "buzzer/buzzer.h"
#include "board_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "buzzer";

esp_err_t buzzer_init(void)
{
    gpio_config_t io_cfg = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << BOARD_BUZZER_GPIO),
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    gpio_set_level(BOARD_BUZZER_GPIO, 0);
    ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", BOARD_BUZZER_GPIO);
    return ESP_OK;
}

void buzzer_beep(uint32_t duration_ms)
{
    gpio_set_level(BOARD_BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    gpio_set_level(BOARD_BUZZER_GPIO, 0);
}
