idf_component_register(
    SRCS
        "main.c"
        "app_state.c"
        "i2c_bus/i2c_bus.c"
        "relay/relay.c"
        "digital_input/digital_input.c"
        "ethernet/ethernet.c"
        "wifi/wifi_manager.c"
        "nvs_config/nvs_config.c"
        "http_server/http_server.c"
        "webhook/webhook.c"
        "ntp/ntp.c"
        "led/led.c"
        "buzzer/buzzer.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_eth
        esp_http_server
        esp_http_client
        esp_driver_i2c
        esp_driver_gpio
        esp_driver_rmt
        esp_driver_spi
        esp_event
        esp_netif
        esp_timer
        cjson
        ethernet_init
        lwip
)

# Embed the single-page web UI HTML directly into the firmware binary
target_add_binary_data(${COMPONENT_LIB} "web_ui/index.html" TEXT)
target_add_binary_data(${COMPONENT_LIB} "web_ui/static/bootstrap/bootstrap.min.css" TEXT)
target_add_binary_data(${COMPONENT_LIB} "web_ui/static/bootstrap/bootstrap.min.js" TEXT)
target_add_binary_data(${COMPONENT_LIB} "web_ui/static/app.js" TEXT)
