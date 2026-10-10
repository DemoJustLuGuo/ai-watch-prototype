#ifndef UI_TIME_H
#define UI_TIME_H

#include "lvgl.h"

lv_obj_t *ui_time_create(void);
void ui_time_set(const char *hhmm, const char *ss);

#endif
