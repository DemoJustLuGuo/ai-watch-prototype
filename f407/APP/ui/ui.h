#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_init(void);
void ui_on_key(uint8_t key);
uint8_t ui_on_voice(void);	/* 1: 当前在语音页 */

/* 游戏任务由 main 提供：进入时建一个，退出时删掉。which: 0 贪吃蛇  1 乒乓球  2 俄罗斯方块 */
void ui_game_start(uint8_t which);
void ui_game_stop(void);

/* main 每拍把真实数据推进来，界面只显示 */
void ui_set_time(uint8_t h, uint8_t m, uint8_t s);
void ui_set_temp(int16_t deci, uint8_t present);	/* deci: 0.1°C */
void ui_set_online(uint8_t online);
void ui_set_steps(uint32_t steps);
void ui_set_today(uint32_t steps);			/* 今日步数，主页和计步页 */
void ui_set_dyn(uint16_t dyn_mg);			/* 校准页的动态量，0.001 g */
void ui_set_weather(const char *line);
void ui_set_voice(const char *text);

/* 告警页。kind: 0 跌倒  1 倒计时到点  2 闹钟到点。 */
void ui_show_alert(uint8_t kind);

/* 上电时把 EEPROM 里的阈值灌进来，校准页以此为初值。 */
void ui_set_calib(uint16_t th_mg, uint16_t gap_ms);

/* 界面不直接写 EEPROM 和 RTC，把动作交回 main。
   save_cb：计步数据需要落盘（清零或改阈值）。
   time_cb：对时页确认，参数是时、分、秒。 */
typedef void (*ui_save_cb_t)(void);
typedef void (*ui_time_cb_t)(uint8_t h, uint8_t m, uint8_t s);
void ui_set_save_cb(ui_save_cb_t cb);
void ui_set_time_cb(ui_time_cb_t cb);

/* 校准页当前的阈值和步间隔，main 保存时读。 */
void ui_calib_params(uint16_t *th_mg, uint16_t *gap_ms);

/* 取走一次保存请求：0 无，1 改了阈值，2 清零。读完即清。 */
uint8_t ui_take_save(void);

#endif
