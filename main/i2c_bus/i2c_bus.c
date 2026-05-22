#include "i2c_bus/i2c_bus.h"
#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";
static i2c_master_bus_handle_t s_bus_handle = NULL;

esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = I2C_NUM_0,
        .scl_io_num        = BOARD_I2C_SCL_GPIO,
        .sda_io_num        = BOARD_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&cfg, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "I2C bus initialized (SCL=%d SDA=%d)", BOARD_I2C_SCL_GPIO, BOARD_I2C_SDA_GPIO);
    }
    return ret;
}

i2c_master_bus_handle_t i2c_bus_get_handle(void)
{
    return s_bus_handle;
}
