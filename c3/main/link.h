#ifndef LINK_H
#define LINK_H

#include <stdint.h>

/* UART 链路：收 F407 的帧，按 TYPE 分发，回 ACK / NACK / 结果。
   发送统一走这里，wifi_mgr 掉线时也用 link_send_hello_ack 主动报离线。 */

void link_start(void);

/* 组一帧 HELLO_ACK 发出。online=0 离线 / 1 已连 AP，ip 为 4 字节 IPv4（离线时传全 0）。 */
void link_send_hello_ack(uint8_t online, const uint8_t ip[4]);

/* 回 ACK，SEQ 回显被确认的那一帧。 */
void link_send_ack(uint8_t seq);

/* 发 NACK，载荷 1 字节错误码。 */
void link_send_nack(uint8_t err);

/* 发一帧 UTF-8 文本（ASR_TEXT 或 WEATHER_RESULT），SEQ 用内部自增。 */
void link_send_text(uint8_t type, const char *text);

#endif
