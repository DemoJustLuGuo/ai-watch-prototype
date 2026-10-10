#ifndef UI_CALIB_H
#define UI_CALIB_H

#include "lvgl.h"
#include <stdint.h>

/* 计步校准页。从计步页进入，不进主页菜单。
   阈值和步间隔的当前值由 main 推进来，按键只改界面上的数字，
   离开时由 ui.c 负责把结果交回去保存。 */

lv_obj_t *ui_calib_create(void);

/* dyn_mg: 本次动态量，0.001 g。传 0xFFFF 表示这次不更新动态量。 */
void ui_calib_set(uint16_t dyn_mg, uint16_t th_mg, uint16_t gap_ms);

void ui_calib_on_key(uint8_t key);

uint16_t ui_calib_threshold(void);
uint16_t ui_calib_gap(void);

#endif
