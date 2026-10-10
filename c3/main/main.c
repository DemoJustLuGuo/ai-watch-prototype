#include "link.h"
#include "wifi_mgr.h"
#include "weather.h"
#include "asr.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

static const char *TAG = "main";

/* 启动横幅：把版本、编译时间、ELF SHA-256 前 8 字节和当前运行分区打出来。
   板上跑的到底是哪次构建，只认这一行，不靠"源码里有这个字符串"推断。
   与 build/ai-watch-c3.elf 的 SHA-256 前 8 字节对照。 */
static void print_banner(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    const esp_partition_t *run = esp_ota_get_running_partition();

    char elf[17];
    for (int i = 0; i < 8; i++) snprintf(elf + i * 2, 3, "%02x", d->app_elf_sha256[i]);

    ESP_LOGI(TAG, "固件 %s %s  编译 %s %s  IDF %s",
             d->project_name, d->version, d->date, d->time, d->idf_ver);
    ESP_LOGI(TAG, "elf %s  运行分区 %s @ 0x%06X  空闲堆 %u 字节",
             elf, run ? run->label : "?", run ? (unsigned)run->address : 0,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* 避免开机天气请求和 F407 的语音识别 HTTPS 请求并发。 */
#define SELFTEST_WEATHER 0

#if SELFTEST_WEATHER
static void selftest_task(void *arg)
{
    uint8_t ip[4];
    for (;;) {
        if (wifi_mgr_is_online(ip)) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "自检：已拿到 IP，请求和风天气");

    char line[96];
    if (weather_fetch(line, sizeof(line))) {
        ESP_LOGI(TAG, "自检通过：%s", line);
    } else {
        ESP_LOGE(TAG, "自检失败：%s", line);
    }
    vTaskDelete(NULL);
}
#endif

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    print_banner();

    asr_init();          /* 先拿到 audio 分区句柄，再开串口收音频帧 */
    link_start();        /* 先开串口，F407 的 HELLO 来了就能回 */
    wifi_mgr_start();

#if SELFTEST_WEATHER
    xTaskCreate(selftest_task, "selftest", 8192, NULL, 4, NULL);
#endif

    ESP_LOGI(TAG, "网关启动");
}
