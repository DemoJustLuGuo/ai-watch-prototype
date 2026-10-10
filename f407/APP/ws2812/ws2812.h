#ifndef _ws2812_H
#define _ws2812_H

#include "system.h"

/* 5x5 WS2812 点阵，数据线 PE5。字母宽 5 列，整屏也只有 5 列，
   所以一次只露出一个字母的一部分，靠横向滚动看全。 */

#define RGB_LED_XWIDTH 5
#define RGB_LED_YHIGH  5

void RGB_LED_Init(void);
void RGB_Scroll_Task(void *arg);	/* 独立任务，约 90 ms 走一列，循环滚 CQUT */

#endif
