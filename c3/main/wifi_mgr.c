#include "wifi_mgr.h"
#include "link.h"
#include "secrets.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi;
static uint8_t s_ip[4];
static bool s_online = false;

/* 掉线时主动报一次离线，让 F407 把状态栏改成「离线」。 */
static void report_offline(void)
{
    uint8_t zero[4] = {0, 0, 0, 0};
    s_online = false;
    link_send_hello_ack(0, zero);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)data;
        if (s_online) report_offline();          /* 从在线掉下来才报一次 */
        s_online = false;
        xEventGroupClearBits(s_wifi, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "断开 reason=%u rssi=%d，5 秒后重连",
                 (unsigned)ev->reason, (int)ev->rssi);
        vTaskDelay(pdMS_TO_TICKS(5000));          /* 方案：掉线后每 5 秒重连 */
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        memcpy(s_ip, &ev->ip_info.ip.addr, 4);    /* addr 已是小端字节序 */
        s_online = true;
        xEventGroupSetBits(s_wifi, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "已连上 " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

void wifi_mgr_start(void)
{
    s_wifi = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_mgr_is_online(uint8_t ip[4])
{
    if (!s_online) return false;
    memcpy(ip, s_ip, 4);
    return true;
}
