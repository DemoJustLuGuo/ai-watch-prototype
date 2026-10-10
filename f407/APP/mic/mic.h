#ifndef _mic_H
#define _mic_H

#include "system.h"

/* 模拟麦克风接 PA5（ADC1_IN5）。上电前 J01、J4 的跳线帽要拔掉，
   否则采到的是电位器或触摸的直流。 */

#define MIC_RATE      16000u
#define MIC_MAX_MS    10000u
#define MIC_MAX_BYTES (MIC_RATE * 2u * MIC_MAX_MS / 1000u)	/* 320000, external SRAM */

void Mic_Init(void);
u8   Mic_Start(void);		/* 1 启动成功，0 初始化/启动失败 */
void Mic_Stop(void);		/* 松开按键时调用，停在当前采样点 */
u8   Mic_Busy(void);
u32  Mic_Bytes(void);		/* 已采到的 PCM 字节数，偶数字节对齐 */
const u8 *Mic_Data(void);	/* 16 bit 小端 PCM，中点在 0 */

#define MIC_ERR_INIT    1u
#define MIC_ERR_START   2u
#define MIC_ERR_ADC     4u
#define MIC_ERR_DMA     8u
#define MIC_ERR_LATE   16u
#define MIC_ERR_MEMORY 32u

u32 Mic_Error(void);
void Mic_Report(void);  /* 停止后从任务调用，输出长度、幅度和错误，不在 ISR 打印 */

#endif
