#ifndef UI_SETTIME_H
#define UI_SETTIME_H

#include "lvgl.h"
#include <stdint.h>

/* 对时页。从时间页进入，改的是界面上的三个数字，
   确认时由 ui.c 调 RTC_Set 写进后备域。 */

lv_obj_t *ui_settime_create(void);

/* 进入页面前用当前时间填初值。 */
void ui_settime_begin(uint8_t h, uint8_t m, uint8_t s);
void ui_settime_on_key(uint8_t key);

uint8_t ui_settime_hour(void);
uint8_t ui_settime_min(void);
uint8_t ui_settime_sec(void);

/* 闹钟。enabled 非 0 时，时分到点由 main 触发蜂鸣。 */
void ui_alarm_set(uint8_t enabled, uint8_t h, uint8_t m);
uint8_t ui_alarm_enabled(void);
uint8_t ui_alarm_hour(void);
uint8_t ui_alarm_min(void);

#endif
