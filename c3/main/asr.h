#ifndef ASR_H
#define ASR_H

#include <stdint.h>
#include <stdbool.h>

/* 接收 F407 的三段音频帧，落到 flash 的 audio 分区，再补 WAV 头流式发给火山方舟，
   把识别出的文字经 ASR_TEXT 发回。每帧都回 ACK，F407 靠这个停等。

   为什么落盘：10 秒 16 kHz/16 bit/单声道是 320000 字节，C3 只有 400 KB SRAM，
   Wi-Fi + TLS 之后剩不下这么大的整段缓冲，更放不下同时存在的 WAV/Base64/JSON。
   上传时按块从 flash 读、按块 Base64、按块写进 HTTPS 请求体，峰值只有几 KB。 */

/* 建 flash 句柄和互斥量，并起一个低优先级任务预擦 audio 分区。
   必须在 link_start 之前调用。 */
void asr_init(void);

void asr_begin(uint32_t total, uint8_t seq);
void asr_data(uint32_t offset, const uint8_t *pcm, uint16_t len, uint8_t seq);
void asr_end(uint8_t seq);

#endif
