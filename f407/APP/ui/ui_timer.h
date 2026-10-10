#ifndef UI_TIMER_H
#define UI_TIMER_H

#include "lvgl.h"
#include <stdint.h>

/* 秒表和倒计时共用一页，下键在两者之间切换。
   秒表按经过的毫秒显示，倒计时按剩余毫秒显示，到 0 由 main 负责蜂鸣。 */

#define TIMER_MODE_STOPWATCH 0u
#define TIMER_MODE_COUNTDOWN 1u

lv_obj_t *ui_timer_create(void);

void ui_timer_on_key(uint8_t key);

/* 返回 1 表示正在计时，main 才需要每 100 ms 推一次。 */
uint8_t ui_timer_running(void);
uint8_t ui_timer_mode(void);

/* 秒表：已累计的毫秒。倒计时：剩余毫秒，减到 0 时页面自己停下。 */
void ui_timer_set(uint32_t ms);

#endif
