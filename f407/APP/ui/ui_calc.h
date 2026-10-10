#ifndef UI_CALC_H
#define UI_CALC_H

#include "lvgl.h"
#include <stdint.h>

/* 四则运算计算器。按键移动光标选键，OK 输入。
   支持小数，连续运算取上一次结果当左操作数。 */

lv_obj_t *ui_calc_create(void);
void ui_calc_on_key(uint8_t key);

#endif
