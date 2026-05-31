#include "relay/relay.h"
#include "i2c_bus/i2c_bus.h"
#include "app_state.h"
#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "relay";
static i2c_master_dev_handle_t s_dev = NULL;

/* Write the output latch register of the TCA9554.
 * Retries a couple of times so a transient bus glitch doesn't silently drop a
 * relay command — important when the relay state is safety-relevant. */
static esp_err_t tca9554_write_output(uint8_t mask)
{
    uint8_t buf[2] = {BOARD_TCA9554_REG_OUT, mask};
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        ret = i2c_master_transmit(s_dev, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (ret == ESP_OK) return ESP_OK;
        ESP_LOGW(TAG, "TCA9554 write failed (attempt %d): %s",
                 attempt + 1, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t relay_init(void)
{
    i2c_device_config_t dev_cfg = {
        .device_address = BOARD_TCA9554_ADDR,
        .scl_speed_hz   = BOARD_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_get_handle(), &dev_cfg, &s_dev));

    /* Configure all 8 pins as outputs */
    uint8_t cfg_cmd[2] = {BOARD_TCA9554_REG_CFG, 0x00};
    ESP_ERROR_CHECK(i2c_master_transmit(s_dev, cfg_cmd, sizeof(cfg_cmd), pdMS_TO_TICKS(100)));

    /* All relays off */
    ESP_ERROR_CHECK(tca9554_write_output(0x00));

    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_state.relay_mask = 0x00;
        xSemaphoreGive(g_state_mutex);
    }
    ESP_LOGI(TAG, "TCA9554 initialized, all relays off");
    return ESP_OK;
}

esp_err_t relay_set(uint8_t relay_num, bool state)
{
    if (relay_num < 1 || relay_num > BOARD_RELAY_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    uint8_t mask = g_state.relay_mask;
    uint8_t bit  = (uint8_t)(1u << (relay_num - 1));
    if (state) {
        mask |= bit;
    } else {
        mask &= ~bit;
    }
    esp_err_t ret = tca9554_write_output(mask);
    if (ret == ESP_OK) {
        g_state.relay_mask = mask;
        ESP_LOGI(TAG, "Relay %d -> %s (mask=0x%02x)", relay_num, state ? "ON" : "OFF", mask);
    }
    xSemaphoreGive(g_state_mutex);
    return ret;
}

uint8_t relay_get_mask(void)
{
    uint8_t mask = 0;
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mask = g_state.relay_mask;
        xSemaphoreGive(g_state_mutex);
    }
    return mask;
}
