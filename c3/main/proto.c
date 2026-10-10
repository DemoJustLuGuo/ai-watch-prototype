#include "proto.h"

/* Modbus CRC16：多项式 0xA001，初值 0xFFFF。可续算，中间值当种子继续喂。 */
static uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else              crc >>= 1;
        }
    }
    return crc;
}

uint16_t proto_crc16(const uint8_t *data, uint16_t len)
{
    return crc16_update(0xFFFF, data, len);
}

uint16_t proto_build(uint8_t *out, uint8_t type, uint8_t seq,
                     const uint8_t *payload, uint16_t len)
{
    if (len > PROTO_MAX_PAYLOAD) len = PROTO_MAX_PAYLOAD;

    out[0] = PROTO_SOF0;
    out[1] = PROTO_SOF1;
    out[2] = PROTO_VER;
    out[3] = PROTO_SRC_C3;   /* 本机是 C3，发往 F407 */
    out[4] = PROTO_SRC_F4;
    out[5] = type;
    out[6] = seq;
    out[7] = (uint8_t)(len & 0xFF);   /* LEN 小端 */
    out[8] = (uint8_t)(len >> 8);
    for (uint16_t i = 0; i < len; i++) out[PROTO_HEAD_LEN + i] = payload[i];

    uint16_t crc = proto_crc16(&out[2], (uint16_t)(7 + len));   /* VER..载荷末字节 */
    out[PROTO_HEAD_LEN + len]     = (uint8_t)(crc & 0xFF);      /* CRC 小端 */
    out[PROTO_HEAD_LEN + len + 1] = (uint8_t)(crc >> 8);
    return (uint16_t)(PROTO_HEAD_LEN + len + 2);
}

/* 接收状态机。状态保存在静态变量里，因为同一条 UART 只有这一路在喂。 */
uint8_t proto_feed(uint8_t byte, proto_frame_t *f, uint8_t *crc_err)
{
    static uint8_t  st = 0;
    static uint8_t  ver, src, dst, type, seq;
    static uint16_t len, idx;
    static uint8_t  buf[PROTO_MAX_PAYLOAD];
    static uint16_t rx_crc;

    *crc_err = 0;

    switch (st) {
    case 0:
        if (byte == PROTO_SOF0) st = 1;
        break;
    case 1:
        if (byte == PROTO_SOF1) st = 2;
        else if (byte == PROTO_SOF0) st = 1;   /* 连续 A5 时留在这里 */
        else st = 0;
        break;
    case 2: ver = byte; st = 3; break;
    case 3: src = byte; st = 4; break;
    case 4: dst = byte; st = 5; break;
    case 5: type = byte; st = 6; break;
    case 6: seq = byte; st = 7; break;
    case 7: len = byte; st = 8; break;                 /* LEN 低字节 */
    case 8:
        len |= (uint16_t)byte << 8;                    /* LEN 高字节 */
        if (len > PROTO_MAX_PAYLOAD) { st = 0; break; }
        idx = 0;
        st = (len > 0) ? 9 : 10;
        break;
    case 9:
        buf[idx++] = byte;
        if (idx >= len) st = 10;
        break;
    case 10: rx_crc = byte; st = 11; break;            /* CRC 低字节 */
    case 11: {
        uint8_t head[7];
        rx_crc |= (uint16_t)byte << 8;                 /* CRC 高字节 */
        st = 0;
        head[0] = ver; head[1] = src; head[2] = dst;
        head[3] = type; head[4] = seq;
        head[5] = (uint8_t)(len & 0xFF); head[6] = (uint8_t)(len >> 8);
        uint16_t calc = crc16_update(0xFFFF, head, 7);
        calc = crc16_update(calc, buf, len);
        if (calc != rx_crc) { *crc_err = 1; return 0; }
        f->src = src; f->dst = dst; f->type = type; f->seq = seq; f->len = len;
        for (idx = 0; idx < len; idx++) f->payload[idx] = buf[idx];
        return 1;
    }
    default: st = 0; break;
    }
    return 0;
}
