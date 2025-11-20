#include "core/timeutil.h"

#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"

#include <time.h>
#include <sys/time.h>

static const char *TAG = "TIMEUTIL";

static bool s_time_set = false;

static void time_sync_cb(struct timeval *tv)
{
    (void)tv;
    s_time_set = true;
    ESP_LOGI(TAG, "Time synchronized via SNTP");
}

void timeutil_init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP...");

    // Set timezone (adjust for your region)
    setenv("TZ", "PST8PDT", 1);
    tzset();

    // New style config
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_cb;
    config.start   = true;  // start client immediately

    // In case it was already initialized somewhere else
    esp_netif_sntp_deinit();

    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));
}

bool timeutil_is_time_set(void)
{
    return s_time_set;
}

bool timeutil_get_iso8601(char *buf, size_t buf_len)
{
    if (!s_time_set || buf_len < 32) {
        return false;
    }

    time_t now_sec;
    time(&now_sec);
    if (now_sec == 0) {
        return false;
    }

    struct tm local_tm;
    localtime_r(&now_sec, &local_tm);

    int n = strftime(buf, buf_len, "%Y-%m-%dT%H:%M:%S%z", &local_tm);
    return (n > 0 && (size_t)n < buf_len);
}
