#ifndef UI_WEATHER_H
#define UI_WEATHER_H

#include "lvgl.h"
#include <stdint.h>

typedef void (*ui_weather_req_cb_t)(const char *city);

lv_obj_t *ui_weather_create(void);
void ui_weather_set(const char *line);
void ui_weather_set_req_cb(ui_weather_req_cb_t cb);	/* main 注入"请求天气"动作，参数是城市名 */
void ui_weather_on_enter(void);
void ui_weather_on_key(uint8_t key);	/* 天气页内的按键，右键循环切换城市 */

#endif
