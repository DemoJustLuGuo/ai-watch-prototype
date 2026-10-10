#ifndef UI_ALERT_H
#define UI_ALERT_H

#include "lvgl.h"

/* 跌倒告警页。触发时由界面任务切过来，任意键清掉。 */

lv_obj_t *ui_alert_create(void);

/* 0 跌倒  1 倒计时到点  2 闹钟到点 */
void ui_alert_set(uint8_t kind);

#endif
