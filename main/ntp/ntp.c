#include "ntp/ntp.h"
#include "nvs_config/nvs_config.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <time.h>
#include <string.h>

static const char *TAG = "ntp";

static void sync_cb(struct timeval *tv)
{
    char buf[32];
    time_t now = tv->tv_sec;
    struct tm t;
    localtime_r(&now, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "Time synced: %s (local)", buf);
}

esp_err_t ntp_init(void)
{
    char server[64] = "pool.ntp.org";
    char tz[64]     = "UTC0";
    nvs_config_get_ntp(server, sizeof(server), tz, sizeof(tz));

    setenv("TZ", tz, 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, server);
    esp_sntp_set_time_sync_notification_cb(sync_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started: server=%s tz=%s", server, tz);
    return ESP_OK;
}
