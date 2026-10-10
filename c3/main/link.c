#include "link.h"
#include "proto.h"
#include "weather.h"
#include "wifi_mgr.h"
#include "asr.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "link";

/* 对 F407 的口走 UART1：TX=GPIO4，RX=GPIO5。
   这块板的 USB 转串口占用了 GPIO20/21（日志里有 "GPIO 20 and 21 are used as
   console UART"），所以 UART0 留给 USB 日志，不拿来传协议。F407 仍是 PC10/PC11。 */
#define LINK_UART   UART_NUM_1
#define LINK_TX_PIN 4
#define LINK_RX_PIN 5
#define LINK_BAUD   460800

static uint8_t s_seq = 0;

static void send_frame(uint8_t type, uint8_t seq, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[PROTO_MAX_FRAME];
    uint16_t n = proto_build(frame, type, seq, payload, len);
    uart_write_bytes(LINK_UART, frame, n);
}

void link_send_hello_ack(uint8_t online, const uint8_t ip[4])
{
    uint8_t p[5];
    p[0] = online ? 1 : 0;
    memcpy(&p[1], ip, 4);
    send_frame(PT_HELLO_ACK, s_seq++, p, 5);
}

void link_send_ack(uint8_t seq)
{
    send_frame(PT_ACK, seq, NULL, 0);
}

void link_send_nack(uint8_t err)
{
    send_frame(PT_NACK, s_seq++, &err, 1);
}

void link_send_text(uint8_t type, const char *text)
{
    send_frame(type, s_seq++, (const uint8_t *)text, (uint16_t)strlen(text));
}

/* 回一帧 HELLO_ACK，在线标志和 IP 取当前真实状态。 */
static void reply_hello(uint8_t seq)
{
    uint8_t ip[4] = {0, 0, 0, 0};
    uint8_t online = wifi_mgr_is_online(ip) ? 1 : 0;
    uint8_t p[5];
    p[0] = online;
    memcpy(&p[1], ip, 4);
    send_frame(PT_HELLO_ACK, seq, p, 5);
}

static void handle_frame(const proto_frame_t *f)
{
    switch (f->type) {
    case PT_HELLO:
        reply_hello(f->seq);                 /* SEQ 回显被应答的那帧 */
        break;

    case PT_CMD_WEATHER: {
        char line[96];
        /* 载荷是城市名（UTF-8，无结尾 0 也可）；空载荷用默认城市。 */
        char city[32];
        if (f->len > 0) {
            uint16_t n = f->len < sizeof(city) - 1 ? f->len : sizeof(city) - 1;
            memcpy(city, f->payload, n);
            city[n] = 0;
            weather_fetch_city(city, line, sizeof(line));
        } else {
            weather_fetch(line, sizeof(line));
        }
        send_frame(PT_WEATHER_RESULT, f->seq, (const uint8_t *)line, (uint16_t)strlen(line));
        break;
    }

    /* 音频三段：BEGIN 报总长度，DATA 按偏移写入，END 触发上传。每帧回 ACK。 */
    case PT_AUDIO_BEGIN: {
        uint32_t total = 0;
        if (f->len >= 4)
            total = (uint32_t)f->payload[0] | ((uint32_t)f->payload[1] << 8) |
                    ((uint32_t)f->payload[2] << 16) | ((uint32_t)f->payload[3] << 24);
        asr_begin(total, f->seq);
        break;
    }
    case PT_AUDIO_DATA: {
        uint32_t offset = 0;
        if (f->len >= 4) {
            offset = (uint32_t)f->payload[0] | ((uint32_t)f->payload[1] << 8) |
                     ((uint32_t)f->payload[2] << 16) | ((uint32_t)f->payload[3] << 24);
            asr_data(offset, f->payload + 4, f->len - 4, f->seq);
        } else {
            link_send_nack(PE_BUSY);
        }
        break;
    }
    case PT_AUDIO_END:
        asr_end(f->seq);
        break;

    case PT_ALERT_FALL:
        ESP_LOGW(TAG, "收到跌倒告警，载荷 %u 字节", f->len);
        break;

    default:
        ESP_LOGW(TAG, "未识别的 TYPE 0x%02X", f->type);
        break;
    }
}

static void link_task(void *arg)
{
    uint8_t byte;
    proto_frame_t frame;
    uint8_t crc_err;

    for (;;) {
        int n = uart_read_bytes(LINK_UART, &byte, 1, pdMS_TO_TICKS(1000));
        if (n <= 0) continue;

        if (proto_feed(byte, &frame, &crc_err)) {
            handle_frame(&frame);
        } else if (crc_err) {
            uint8_t err = PE_CRC;
            ESP_LOGW(TAG, "CRC 错，回 NACK");
            send_frame(PT_NACK, s_seq++, &err, 1);
        }
    }
}

void link_start(void)
{
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX_PIN, LINK_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(link_task, "link", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART%d  %d 8N1  TX=GPIO%d RX=GPIO%d",
             LINK_UART, LINK_BAUD, LINK_TX_PIN, LINK_RX_PIN);
}
