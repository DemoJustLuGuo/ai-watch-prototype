#ifndef WIFI_MGR_H
#define WIFI_MGR_H

#include <stdbool.h>
#include <stdint.h>

/* 连接 secrets.h 里的热点。掉线后每 5 秒重连，掉线时经 link 主动发一次离线的 HELLO_ACK。 */

void wifi_mgr_start(void);

/* 已拿到 IP 返回 true，并把 4 字节地址写入 ip。 */
bool wifi_mgr_is_online(uint8_t ip[4]);

#endif
