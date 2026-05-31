#include "nvs_config/nvs_config.h"
#include "board_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

static const char *TAG       = "nvs_cfg";
static const char *NAMESPACE = "app_cfg";

/*
 * All config is mirrored in this RAM cache so the hot paths (webhook dispatch on
 * every input edge, REST API reads) never touch flash. NVS is read once at init
 * and written only when a setter runs; the cache is the single source of truth
 * for reads. s_cfg_mutex guards concurrent access from httpd threads + the
 * webhook task.
 */
typedef struct {
    char        wifi_ssid[33];
    char        wifi_pass[65];
    char        wh_url[BOARD_DI_COUNT][256];
    uint8_t     wh_trig[BOARD_DI_COUNT];
    uint8_t     wh_enabled[BOARD_DI_COUNT];
    uint8_t     di_mode[BOARD_DI_COUNT];
    uint8_t     di_relay[BOARD_DI_COUNT];
    char        ntp_server[64];
    char        ntp_tz[64];
    nvs_ipcfg_t ipcfg;
} app_cfg_t;

static app_cfg_t        s_cfg;
static SemaphoreHandle_t s_cfg_mutex = NULL;

static const char * const s_url_keys[]  = {"wh1_url","wh2_url","wh3_url","wh4_url",
                                           "wh5_url","wh6_url","wh7_url","wh8_url"};
static const char * const s_trig_keys[] = {"wh1_trig","wh2_trig","wh3_trig","wh4_trig",
                                           "wh5_trig","wh6_trig","wh7_trig","wh8_trig"};
static const char * const s_whe_keys[]  = {"di1_whe","di2_whe","di3_whe","di4_whe",
                                           "di5_whe","di6_whe","di7_whe","di8_whe"};
static const char * const s_mode_keys[] = {"di1_mode","di2_mode","di3_mode","di4_mode",
                                           "di5_mode","di6_mode","di7_mode","di8_mode"};
static const char * const s_rly_keys[]  = {"di1_rly","di2_rly","di3_rly","di4_rly",
                                           "di5_rly","di6_rly","di7_rly","di8_rly"};

#define CFG_LOCK()   xSemaphoreTake(s_cfg_mutex, portMAX_DELAY)
#define CFG_UNLOCK() xSemaphoreGive(s_cfg_mutex)

/* ---- cache load (init) ---- */

static void load_str(nvs_handle_t h, const char *key, char *dst, size_t len, const char *def)
{
    size_t l = len;
    if (nvs_get_str(h, key, dst, &l) != ESP_OK) {
        strlcpy(dst, def, len);
    }
}

static void load_u8(nvs_handle_t h, const char *key, uint8_t *dst, uint8_t def)
{
    if (nvs_get_u8(h, key, dst) != ESP_OK) *dst = def;
}

static void cache_load(void)
{
    /* Start from defaults so a missing namespace (first boot) yields sane values */
    memset(&s_cfg, 0, sizeof(s_cfg));
    strlcpy(s_cfg.ntp_server, "pool.ntp.org", sizeof(s_cfg.ntp_server));
    strlcpy(s_cfg.ntp_tz,     "UTC0",         sizeof(s_cfg.ntp_tz));
    s_cfg.ipcfg.dhcp = 1;
    for (int i = 0; i < BOARD_DI_COUNT; i++) {
        s_cfg.wh_trig[i]    = NVS_WEBHOOK_TRIG_RISING;
        s_cfg.wh_enabled[i] = 1;
        s_cfg.di_mode[i]    = NVS_INPUT_MODE_OFF;
        s_cfg.di_relay[i]   = (uint8_t)(1 << i);
    }

    nvs_handle_t h;
    if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "No stored config — using defaults");
        return;
    }
    load_str(h, "wifi_ssid", s_cfg.wifi_ssid, sizeof(s_cfg.wifi_ssid), "");
    load_str(h, "wifi_pass", s_cfg.wifi_pass, sizeof(s_cfg.wifi_pass), "");
    for (int i = 0; i < BOARD_DI_COUNT; i++) {
        load_str(h, s_url_keys[i],  s_cfg.wh_url[i], sizeof(s_cfg.wh_url[i]), "");
        load_u8(h,  s_trig_keys[i], &s_cfg.wh_trig[i],    NVS_WEBHOOK_TRIG_RISING);
        load_u8(h,  s_whe_keys[i],  &s_cfg.wh_enabled[i], 1);
        load_u8(h,  s_mode_keys[i], &s_cfg.di_mode[i],    NVS_INPUT_MODE_OFF);
        load_u8(h,  s_rly_keys[i],  &s_cfg.di_relay[i],   (uint8_t)(1 << i));
    }
    load_str(h, "ntp_server", s_cfg.ntp_server, sizeof(s_cfg.ntp_server), "pool.ntp.org");
    load_str(h, "ntp_tz",     s_cfg.ntp_tz,     sizeof(s_cfg.ntp_tz),     "UTC0");
    load_u8(h,  "net_dhcp", &s_cfg.ipcfg.dhcp, 1);
    s_cfg.ipcfg.dhcp = s_cfg.ipcfg.dhcp ? 1 : 0;
    load_str(h, "net_ip",   s_cfg.ipcfg.ip,      sizeof(s_cfg.ipcfg.ip),      "");
    load_str(h, "net_mask", s_cfg.ipcfg.netmask, sizeof(s_cfg.ipcfg.netmask), "");
    load_str(h, "net_gw",   s_cfg.ipcfg.gateway, sizeof(s_cfg.ipcfg.gateway), "");
    load_str(h, "net_dns",  s_cfg.ipcfg.dns,     sizeof(s_cfg.ipcfg.dns),     "");
    nvs_close(h);
    ESP_LOGI(TAG, "Config cache loaded");
}

esp_err_t nvs_config_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition invalid, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_cfg_mutex = xSemaphoreCreateMutex();
    configASSERT(s_cfg_mutex != NULL);
    cache_load();

    ESP_LOGI(TAG, "NVS initialized");
    return ESP_OK;
}

/* ---- WiFi ---- */

esp_err_t nvs_config_get_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    CFG_LOCK();
    strlcpy(ssid, s_cfg.wifi_ssid, ssid_len);
    strlcpy(pass, s_cfg.wifi_pass, pass_len);
    bool have = s_cfg.wifi_ssid[0] != '\0';
    CFG_UNLOCK();
    return have ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_config_set_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, "wifi_ssid", ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, "wifi_pass", pass));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    strlcpy(s_cfg.wifi_ssid, ssid, sizeof(s_cfg.wifi_ssid));
    strlcpy(s_cfg.wifi_pass, pass, sizeof(s_cfg.wifi_pass));
    CFG_UNLOCK();
    ESP_LOGI(TAG, "WiFi credentials saved (ssid=%s)", ssid);
    return ESP_OK;
}

/* ---- Webhooks ---- */

esp_err_t nvs_config_get_webhook(int input_idx, char *url, size_t url_len, uint8_t *trigger)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    CFG_LOCK();
    strlcpy(url, s_cfg.wh_url[input_idx], url_len);
    *trigger = s_cfg.wh_trig[input_idx];
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_webhook(int input_idx, const char *url, uint8_t trigger)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, s_url_keys[input_idx],  url));
    ESP_ERROR_CHECK(nvs_set_u8(h,  s_trig_keys[input_idx], trigger));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    strlcpy(s_cfg.wh_url[input_idx], url, sizeof(s_cfg.wh_url[input_idx]));
    s_cfg.wh_trig[input_idx] = trigger;
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_get_webhook_enabled(int input_idx, uint8_t *enabled)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    CFG_LOCK();
    *enabled = s_cfg.wh_enabled[input_idx];
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_webhook_enabled(int input_idx, uint8_t enabled)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_whe_keys[input_idx], enabled));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    s_cfg.wh_enabled[input_idx] = enabled;
    CFG_UNLOCK();
    return ESP_OK;
}

/* ---- Input modes ---- */

esp_err_t nvs_config_get_input_mode(int input_idx, uint8_t *mode)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    CFG_LOCK();
    *mode = s_cfg.di_mode[input_idx];
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_input_mode(int input_idx, uint8_t mode)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_mode_keys[input_idx], mode));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    s_cfg.di_mode[input_idx] = mode;
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_get_relay_target(int input_idx, uint8_t *relay_num)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    CFG_LOCK();
    *relay_num = s_cfg.di_relay[input_idx];
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_relay_target(int input_idx, uint8_t relay_num)
{
    if (input_idx < 0 || input_idx >= BOARD_DI_COUNT) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h, s_rly_keys[input_idx], relay_num));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    s_cfg.di_relay[input_idx] = relay_num;
    CFG_UNLOCK();
    return ESP_OK;
}

/* ---- NTP ---- */

esp_err_t nvs_config_get_ntp(char *server, size_t server_len, char *tz, size_t tz_len)
{
    CFG_LOCK();
    strlcpy(server, s_cfg.ntp_server, server_len);
    strlcpy(tz,     s_cfg.ntp_tz,     tz_len);
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_ntp(const char *server, const char *tz)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, "ntp_server", server));
    ESP_ERROR_CHECK(nvs_set_str(h, "ntp_tz",     tz));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    strlcpy(s_cfg.ntp_server, server, sizeof(s_cfg.ntp_server));
    strlcpy(s_cfg.ntp_tz,     tz,     sizeof(s_cfg.ntp_tz));
    CFG_UNLOCK();
    ESP_LOGI(TAG, "NTP config saved: server=%s tz=%s", server, tz);
    return ESP_OK;
}

/* ---- Ethernet IP config ---- */

esp_err_t nvs_config_get_ipcfg(nvs_ipcfg_t *cfg)
{
    CFG_LOCK();
    *cfg = s_cfg.ipcfg;
    CFG_UNLOCK();
    return ESP_OK;
}

esp_err_t nvs_config_set_ipcfg(const nvs_ipcfg_t *cfg)
{
    /* net_dhcp: 1 = DHCP, 0 = static (matches the runtime dhcp field). */
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NAMESPACE, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_u8(h,  "net_dhcp", cfg->dhcp ? 1 : 0));
    ESP_ERROR_CHECK(nvs_set_str(h, "net_ip",   cfg->ip));
    ESP_ERROR_CHECK(nvs_set_str(h, "net_mask", cfg->netmask));
    ESP_ERROR_CHECK(nvs_set_str(h, "net_gw",   cfg->gateway));
    ESP_ERROR_CHECK(nvs_set_str(h, "net_dns",  cfg->dns));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    CFG_LOCK();
    s_cfg.ipcfg = *cfg;
    s_cfg.ipcfg.dhcp = cfg->dhcp ? 1 : 0;
    CFG_UNLOCK();
    ESP_LOGI(TAG, "IP config saved: %s%s", cfg->dhcp ? "DHCP" : "static ", cfg->dhcp ? "" : cfg->ip);
    return ESP_OK;
}
