#include "weather.h"
#include "secrets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "weather";

#define WEATHER_FAIL "天气不可用"

/* 把查询参数里的非 ASCII 字节转成 %XX。 */
static void url_encode(const char *src, char *dst, int dst_len)
{
    static const char hex[] = "0123456789ABCDEF";
    int j = 0;
    for (int i = 0; src[i] && j < dst_len - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            dst[j++] = (char)c;
        } else if (j < dst_len - 3) {
            dst[j++] = '%';
            dst[j++] = hex[c >> 4];
            dst[j++] = hex[c & 0x0F];
        }
    }
    dst[j] = 0;
}

/* GET 一次，正文读进 body。HTTP 200 返回读到的字节数，否则 -1。 */
static int https_get(const char *url, char *body, int body_len)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;

    int got = -1;
    if (esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        got = 0;
        while (got < body_len - 1) {
            int n = esp_http_client_read(c, body + got, body_len - 1 - got);
            if (n <= 0) break;
            got += n;
        }
        body[got] = 0;
        if (esp_http_client_get_status_code(c) != 200) got = -1;
        esp_http_client_close(c);
    }
    esp_http_client_cleanup(c);
    return got;
}

/* 风向角度（0 为正北，顺时针）收成八方位。 */
static const char *wind_dir_text(double deg)
{
    static const char *name[] = {"北风", "东北风", "东风", "东南风",
                                 "南风", "西南风", "西风", "西北风"};
    int i = (int)((deg + 22.5) / 45.0);
    if (i < 0) i = 0;
    return name[i & 7];
}
static const char *code_to_text(int code)
{
    switch (code) {
    case 0:  return "晴";
    case 1:  return "大部晴";
    case 2:  return "多云";
    case 3:  return "阴";
    case 45: case 48: return "雾";
    case 51: case 53: case 55: return "毛毛雨";
    case 61: case 63: case 65: case 80: case 81: case 82: return "雨";
    case 71: case 73: case 75: return "雪";
    case 95: case 96: case 99: return "雷暴";
    default: return "天气";
    }
}

/* 用中文城市名向 Open-Meteo 要经纬度。 */
static bool geocode(const char *city, double *lat, double *lon)
{
    char enc[96], url[256], body[1024];
    url_encode(city, enc, sizeof(enc));
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=zh",
             enc);

    if (https_get(url, body, sizeof(body)) < 0) return false;

    cJSON *root = cJSON_Parse(body);
    if (!root) return false;
    cJSON *results = cJSON_GetObjectItem(root, "results");
    cJSON *first = (cJSON_IsArray(results) && cJSON_GetArraySize(results) > 0)
                       ? cJSON_GetArrayItem(results, 0) : NULL;
    cJSON *la = first ? cJSON_GetObjectItem(first, "latitude") : NULL;
    cJSON *lo = first ? cJSON_GetObjectItem(first, "longitude") : NULL;
    bool ok = cJSON_IsNumber(la) && cJSON_IsNumber(lo);
    if (ok) { *lat = la->valuedouble; *lon = lo->valuedouble; }
    cJSON_Delete(root);
    return ok;
}

bool weather_fetch_city(const char *city, char *out, int out_len)
{
    snprintf(out, out_len, "%s", WEATHER_FAIL);
    if (!city || !city[0]) city = WEATHER_DEFAULT_CITY;

    double lat, lon;
    if (!geocode(city, &lat, &lon)) {
        ESP_LOGE(TAG, "找不到城市: %s", city);
        return false;
    }

    char url[256], body[1024];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
             "wind_direction_10m,weather_code&timezone=auto", lat, lon);

    if (https_get(url, body, sizeof(body)) < 0) return false;

    cJSON *root = cJSON_Parse(body);
    if (!root) return false;
    cJSON *cur = cJSON_GetObjectItem(root, "current");
    cJSON *temp = cur ? cJSON_GetObjectItem(cur, "temperature_2m") : NULL;
    cJSON *feel = cur ? cJSON_GetObjectItem(cur, "apparent_temperature") : NULL;
    cJSON *hum  = cur ? cJSON_GetObjectItem(cur, "relative_humidity_2m") : NULL;
    cJSON *wdir = cur ? cJSON_GetObjectItem(cur, "wind_direction_10m") : NULL;
    cJSON *code = cur ? cJSON_GetObjectItem(cur, "weather_code") : NULL;

    bool ok = false;
    if (cJSON_IsNumber(temp) && cJSON_IsNumber(code)) {
        double v = temp->valuedouble;
        int t = (int)(v + (v >= 0 ? 0.5 : -0.5));
        int f = cJSON_IsNumber(feel) ? (int)(feel->valuedouble + (feel->valuedouble >= 0 ? 0.5 : -0.5)) : t;
        int h = cJSON_IsNumber(hum) ? (int)(hum->valuedouble + 0.5) : 0;
        const char *wd = cJSON_IsNumber(wdir) ? wind_dir_text(wdir->valuedouble) : "";
        snprintf(out, out_len, "%s %s %d°C 体感%d 湿度%d%% %s",
                 city, code_to_text((int)code->valuedouble), t, f, h, wd);
        ESP_LOGI(TAG, "%s", out);
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

bool weather_fetch(char *out, int out_len)
{
    return weather_fetch_city(WEATHER_DEFAULT_CITY, out, out_len);
}
