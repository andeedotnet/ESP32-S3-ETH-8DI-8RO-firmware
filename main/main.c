#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "mdns.h"

#include "app_state.h"
#include "nvs_config/nvs_config.h"
#include "i2c_bus/i2c_bus.h"
#include "relay/relay.h"
#include "digital_input/digital_input.h"
#include "ethernet/ethernet.h"
#include "wifi/wifi_manager.h"
#include "webhook/webhook.h"
#include "http_server/http_server.h"
#include "ntp/ntp.h"
#include "led/led.h"
#include "buzzer/buzzer.h"
#include "health/health.h"

static const char *TAG = "main";

/* Advertise the device as relay.local + an _http._tcp service so it can be
 * reached without knowing the DHCP-assigned IP. Best-effort: failures are logged
 * but never abort boot. */
static void mdns_start(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set("relay");
    mdns_instance_name_set("ESP32 Relay Board");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS started: http://relay.local");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_config_init());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    app_state_init();

    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(relay_init());
    ESP_ERROR_CHECK(buzzer_init());
    ESP_ERROR_CHECK(led_init());
    led_set_pattern(LED_BOOTING);

    ESP_ERROR_CHECK(di_init());

    /* Apply latching input states before webhook task starts */
    webhook_apply_boot_states();

    ESP_ERROR_CHECK(eth_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(ntp_init());
    ESP_ERROR_CHECK(webhook_task_start());
    ESP_ERROR_CHECK(http_server_start());
    mdns_start();
    ESP_ERROR_CHECK(health_task_start());

    buzzer_beep(100);
    ESP_LOGI(TAG, "Relay board ready");
}
