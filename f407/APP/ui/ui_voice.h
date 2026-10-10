#ifndef UI_VOICE_H
#define UI_VOICE_H

#include "lvgl.h"

lv_obj_t *ui_voice_create(void);
void ui_voice_set(const char *text);	/* 状态或 C3 返回的回答，原样显示 */

#endif
