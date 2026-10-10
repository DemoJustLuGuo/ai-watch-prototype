#ifndef _proto_H
#define _proto_H

#include "system.h"

/* F407 <-> ESP32-C3 帧格式，方案 5.1。多字节整数一律小端。
   SOF(2) VER(1) SRC(1) DST(1) TYPE(1) SEQ(1) LEN(2) PAYLOAD(LEN) CRC(2)
   CRC 为 Modbus CRC16(0xA001/0xFFFF)，覆盖 VER..载荷末字节，不含 SOF。 */

#define PROTO_SOF0	0xA5
#define PROTO_SOF1	0x5A
#define PROTO_VER	0x01
#define PROTO_SRC_F4	0xF4
#define PROTO_SRC_C3	0xC3

//TYPE，方案 5.2
#define PT_HELLO			0x01
#define PT_HELLO_ACK		0x02
#define PT_AUDIO_BEGIN		0x10
#define PT_AUDIO_DATA		0x11
#define PT_AUDIO_END		0x12
#define PT_ASR_TEXT			0x13
#define PT_CMD_WEATHER		0x20
#define PT_WEATHER_RESULT	0x21
#define PT_ALERT_FALL		0x30
#define PT_ACK				0x7E
#define PT_NACK				0x7F

//NACK 错误码
#define PE_TIMEOUT	0x01
#define PE_CRC		0x02
#define PE_NOWIFI	0x03
#define PE_HTTP		0x04
#define PE_ASR_EMPTY	0x05
#define PE_BUSY		0x06

#define PROTO_MAX_PAYLOAD	512
#define PROTO_HEAD_LEN		9	//SOF..LEN
#define PROTO_MAX_FRAME		(PROTO_HEAD_LEN + PROTO_MAX_PAYLOAD + 2)

//解析出来的一帧
typedef struct
{
	u8  src;
	u8  dst;
	u8  type;
	u8  seq;
	u16 len;
	u8  payload[PROTO_MAX_PAYLOAD];
} proto_frame_t;

u16 proto_crc16(const u8 *data, u16 len);

//组一帧到 out，返回总长度。payload 可为 NULL（len=0）
u16 proto_build(u8 *out, u8 type, u8 seq, const u8 *payload, u16 len);

/* 逐字节喂入接收状态机。收全一帧且 CRC 正确时返回 1，并填 f；否则返回 0。
   CRC 错误时返回 0，通过 *crc_err 置 1，调用方据此回 NACK。 */
u8 proto_feed(u8 byte, proto_frame_t *f, u8 *crc_err);

#endif
