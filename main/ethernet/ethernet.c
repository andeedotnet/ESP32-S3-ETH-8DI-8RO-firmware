#include "ethernet/ethernet.h"
#include "app_state.h"
#include "led/led.h"
#include "nvs_config/nvs_config.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "ethernet_init.h"
#include <string.h>

static const char *TAG = "eth";

static void eth_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == ETH_EVENT) {
        switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet link up");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet link down");
            if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_state.eth_connected = false;
                g_state.eth_ip[0]    = '\0';
                xSemaphoreGive(g_state_mutex);
            }
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_state.eth_connected = true;
            snprintf(g_state.eth_ip, sizeof(g_state.eth_ip), IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(g_state_mutex);
        }
        ESP_LOGI(TAG, "Ethernet IP: %s", g_state.eth_ip);
        led_set_pattern(LED_CONNECTED);
    }
}

/* Apply a stored static IP config to the Ethernet netif before the link starts.
 * No-op (leaves the DHCP client running) when configured for DHCP or when the
 * stored address is incomplete. */
static void eth_apply_ip_config(esp_netif_t *netif)
{
    nvs_ipcfg_t cfg;
    nvs_config_get_ipcfg(&cfg);
    if (cfg.dhcp) {
        ESP_LOGI(TAG, "Ethernet: DHCP");
        return;
    }
    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr      = esp_ip4addr_aton(cfg.ip);
    ip_info.netmask.addr = esp_ip4addr_aton(cfg.netmask);
    ip_info.gw.addr      = esp_ip4addr_aton(cfg.gateway);
    if (ip_info.ip.addr == 0 || ip_info.netmask.addr == 0) {
        ESP_LOGW(TAG, "Static IP config incomplete (ip/netmask) — falling back to DHCP");
        return;
    }

    esp_netif_dhcpc_stop(netif);  /* harmless if already stopped */
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

    uint32_t dns_addr = esp_ip4addr_aton(cfg.dns);
    if (dns_addr != 0) {
        esp_netif_dns_info_t dns = {0};
        dns.ip.type            = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = dns_addr;
        esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    }
    ESP_LOGI(TAG, "Ethernet: static IP %s mask %s gw %s", cfg.ip, cfg.netmask, cfg.gateway);
}

esp_err_t eth_init(void)
{
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = NULL;

    /* ethernet_init_all() reads GPIO config from sdkconfig.defaults Kconfig values */
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));
    ESP_LOGI(TAG, "Found %d Ethernet port(s)", eth_port_cnt);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handles[0]);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    eth_apply_ip_config(eth_netif);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT,  ESP_EVENT_ANY_ID,   eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_ETH_GOT_IP, eth_event_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
    ESP_LOGI(TAG, "W5500 Ethernet started");
    return ESP_OK;
}

bool eth_is_connected(void)
{
    bool connected = false;
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        connected = g_state.eth_connected;
        xSemaphoreGive(g_state_mutex);
    }
    return connected;
}
