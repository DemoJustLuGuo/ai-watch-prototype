#ifndef UI_HOME_H
#define UI_HOME_H

#include "lvgl.h"

lv_obj_t *ui_home_create(void);
void ui_home_set_focus(uint8_t index);
uint8_t ui_home_focus(void);
void ui_home_set_today(uint32_t steps);	/* 计步行右侧的今日步数 */

#endif
