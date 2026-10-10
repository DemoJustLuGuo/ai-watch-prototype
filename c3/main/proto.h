#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

/* F407 <-> ESP32-C3 帧，方案 5.1。多字节整数一律小端。
   SOF(2) VER(1) SRC(1) DST(1) TYPE(1) SEQ(1) LEN(2) PAYLOAD(LEN) CRC(2)
   CRC 为 Modbus CRC16(0xA001/0xFFFF)，覆盖 VER..载荷末字节，不含 SOF。
   与 F407 侧 APP/proto 同一套规则，两边各自实现，不共享源文件。 */

#define PROTO_SOF0  0xA5
#define PROTO_SOF1  0x5A
#define PROTO_VER   0x01
#define PROTO_SRC_F4 0xF4
#define PROTO_SRC_C3 0xC3

#define PT_HELLO           0x01
#define PT_HELLO_ACK       0x02
#define PT_AUDIO_BEGIN     0x10
#define PT_AUDIO_DATA      0x11
#define PT_AUDIO_END       0x12
#define PT_ASR_TEXT        0x13
#define PT_CMD_WEATHER     0x20
#define PT_WEATHER_RESULT  0x21
#define PT_ALERT_FALL      0x30
#define PT_ACK             0x7E
#define PT_NACK            0x7F

#define PE_TIMEOUT   0x01
#define PE_CRC       0x02
#define PE_NOWIFI    0x03
#define PE_HTTP      0x04
#define PE_ASR_EMPTY 0x05
#define PE_BUSY      0x06

#define PROTO_MAX_PAYLOAD 512
#define PROTO_HEAD_LEN    9
#define PROTO_MAX_FRAME   (PROTO_HEAD_LEN + PROTO_MAX_PAYLOAD + 2)

typedef struct
{
    uint8_t  src;
    uint8_t  dst;
    uint8_t  type;
    uint8_t  seq;
    uint16_t len;
    uint8_t  payload[PROTO_MAX_PAYLOAD];
} proto_frame_t;

uint16_t proto_crc16(const uint8_t *data, uint16_t len);

/* 组一帧到 out，返回总长度。payload 可为 NULL（len=0）。SRC 固定为 C3。 */
uint16_t proto_build(uint8_t *out, uint8_t type, uint8_t seq,
                     const uint8_t *payload, uint16_t len);

/* 逐字节喂入。收全一帧且 CRC 正确返回 1 并填 f；CRC 错返回 0 且 *crc_err=1。 */
uint8_t proto_feed(uint8_t byte, proto_frame_t *f, uint8_t *crc_err);

#endif
