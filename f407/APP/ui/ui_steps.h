#ifndef UI_STEPS_H
#define UI_STEPS_H

#include "lvgl.h"

lv_obj_t *ui_steps_create(void);
void ui_steps_set(uint32_t steps);
void ui_steps_set_today(uint32_t steps);

#endif
