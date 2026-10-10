#ifndef UI_GAMES_H
#define UI_GAMES_H

#include "lvgl.h"
#include <stdint.h>

/* 游戏选择页，以及三个游戏页。离开游戏页时各自停掉定时器。 */

lv_obj_t *ui_games_create(void);
void ui_games_on_key(uint8_t key);
uint8_t ui_games_focus(void);	/* 0 贪吃蛇  1 乒乓球  2 俄罗斯方块 */

lv_obj_t *ui_snake_create(void);
lv_obj_t *ui_snake_ensure(void);	/* 第一次进入时才建界面 */
void ui_snake_on_key(uint8_t key);
void ui_snake_stop(void);
void ui_snake_step(void);	/* 由贪吃蛇自己的任务调用，界面任务里执行 */

lv_obj_t *ui_pong_create(void);
lv_obj_t *ui_pong_ensure(void);
void ui_pong_on_key(uint8_t key);
void ui_pong_stop(void);
void ui_pong_step(void);

lv_obj_t *ui_tetris_create(void);
lv_obj_t *ui_tetris_ensure(void);
void ui_tetris_on_key(uint8_t key);
void ui_tetris_stop(void);
void ui_tetris_step(void);

#endif
