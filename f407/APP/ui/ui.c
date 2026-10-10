#include "ui.h"
#include "ui_internal.h"
#include "ui_home.h"
#include "ui_time.h"
#include "ui_steps.h"
#include "ui_weather.h"
#include "ui_voice.h"
#include "ui_calib.h"
#include "ui_settime.h"
#include "ui_alert.h"
#include "ui_timer.h"
#include "ui_calc.h"
#include "ui_games.h"
#include "voice.h"
#include "key.h"

#define UI_PAGE_HOME    0
#define UI_PAGE_SUB     1
#define UI_PAGE_CALIB   2
#define UI_PAGE_SETTIME 3
#define UI_PAGE_ALERT   4
#define UI_PAGE_TIMER   5
#define UI_PAGE_CALC    6
#define UI_PAGE_GAMES   7
#define UI_PAGE_SNAKE   8
#define UI_PAGE_PONG    9
#define UI_PAGE_TETRIS  10

static lv_obj_t *scr_home;
static lv_obj_t *scr_sub[4];
static lv_obj_t *scr_calib;
static lv_obj_t *scr_settime;
static lv_obj_t *scr_alert;
static lv_obj_t *scr_timer;
static lv_obj_t *scr_calc;
static lv_obj_t *scr_games;

/* 状态栏每页各有一份，用小注册表统一更新（最多 8 页份） */
static lv_obj_t *clock_lbl[12];
static lv_obj_t *temp_lbl[12];
static lv_obj_t *net_lbl[12];
static uint8_t clock_n, temp_n, net_n;

static uint8_t page;
static uint8_t online_state;

/* 最近一次时间，进对时页时当初值。 */
static uint8_t cur_h, cur_m, cur_s;

/* 校准页改过的阈值，离开时交给 main 保存。 */
static uint16_t calib_th, calib_gap;
static uint8_t calib_dirty;

/* main 注入的动作。界面任务不碰 I2C 和 RTC 寄存器。 */
static ui_save_cb_t save_cb;
static ui_time_cb_t time_cb;

void ui_set_save_cb(ui_save_cb_t cb) { save_cb = cb; }
void ui_set_time_cb(ui_time_cb_t cb) { time_cb = cb; }

void ui_style_screen(lv_obj_t *scr)
{
	lv_obj_set_style_bg_color(scr, UI_BG, 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_status_create(lv_obj_t *parent)
{
	lv_obj_t *bar;
	lv_obj_t *clock;
	lv_obj_t *temp;
	lv_obj_t *net;

	bar = lv_obj_create(parent);
	lv_obj_set_size(bar, 320, 36);
	lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(bar, 0, 0);
	lv_obj_set_style_pad_all(bar, 0, 0);
	lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

	clock = lv_label_create(bar);
	lv_obj_set_style_text_font(clock, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(clock, UI_FG, 0);
	lv_label_set_text(clock, "12:00");
	lv_obj_align(clock, LV_ALIGN_LEFT_MID, 12, 0);
	if(clock_n < 8) clock_lbl[clock_n++] = clock;

	temp = lv_label_create(bar);
	lv_obj_set_style_text_font(temp, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(temp, UI_FG, 0);
	lv_label_set_text(temp, "\xE8\xA1\xA8\xE6\xB8\xA9 --");	/* 表温 -- */
	lv_obj_align(temp, LV_ALIGN_CENTER, 0, 0);
	if(temp_n < 8) temp_lbl[temp_n++] = temp;

	net = lv_label_create(bar);
	lv_obj_set_style_text_font(net, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(net, UI_OFFLINE, 0);
	lv_label_set_text(net, "\xE7\xA6\xBB\xE7\xBA\xBF");		/* 离线 */
	lv_obj_align(net, LV_ALIGN_RIGHT_MID, -12, 0);
	if(net_n < 8) net_lbl[net_n++] = net;
	return bar;
}

void ui_footer_create(lv_obj_t *parent, const char *text)
{
	lv_obj_t *foot = lv_label_create(parent);

	/* 箭头走 lv_font_chinese_16 的 fallback（montserrat），汉字走本体，一个标签就够。 */
	lv_obj_set_style_text_font(foot, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(foot, UI_MUTED, 0);
	lv_label_set_text(foot, text);
	lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -12);
}

lv_obj_t *ui_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
	lv_obj_t *card = lv_obj_create(parent);

	lv_obj_remove_style_all(card);
	lv_obj_set_size(card, w, h);
	lv_obj_set_style_bg_color(card, UI_CARD, 0);
	lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(card, 16, 0);
	lv_obj_set_style_border_width(card, 0, 0);
	lv_obj_set_style_pad_all(card, 0, 0);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
	return card;
}

/* ---- 供 main 推数据的 setter，界面只做显示 ---- */

void ui_set_time(uint8_t h, uint8_t m, uint8_t s)
{
	char hhmm[6];
	char ss[4];
	uint8_t i;

	cur_h = h; cur_m = m; cur_s = s;

	hhmm[0]=(char)('0'+h/10u); hhmm[1]=(char)('0'+h%10u); hhmm[2]=':';
	hhmm[3]=(char)('0'+m/10u); hhmm[4]=(char)('0'+m%10u); hhmm[5]=0;
	ss[0]=(char)('0'+s/10u); ss[1]=(char)('0'+s%10u); ss[2]=0;

	for(i=0;i<clock_n;i++) lv_label_set_text(clock_lbl[i], hhmm);
	ui_time_set(hhmm, ss);
}

void ui_set_temp(int16_t deci, uint8_t present)
{
	char buf[16];
	uint8_t i;

	if(present)
	{
		int16_t a=deci; uint8_t neg=0;
		if(a<0){neg=1;a=(int16_t)-a;}
		/* 表温 xx.x°C，°C 用 montserrat 的 U+00B0 + 'C' */
		lv_snprintf(buf,sizeof(buf),"%s%d.%d\xC2\xB0""C", neg?"-":"", a/10, a%10);
	}
	else
	{
		lv_snprintf(buf,sizeof(buf),"\xE8\xA1\xA8\xE6\xB8\xA9 --");	/* 表温 -- */
	}
	for(i=0;i<temp_n;i++) lv_label_set_text(temp_lbl[i], buf);
}

void ui_set_online(uint8_t online)
{
	uint8_t i;
	const char *txt = online ? "\xE5\x9C\xA8\xE7\xBA\xBF" : "\xE7\xA6\xBB\xE7\xBA\xBF"; /* 在线 / 离线 */
	lv_color_t col = online ? UI_ACCENT : UI_OFFLINE;

	if(online==online_state && net_n>0) return;	/* 没变就不重画 */
	online_state=online;
	for(i=0;i<net_n;i++)
	{
		lv_label_set_text(net_lbl[i], txt);
		lv_obj_set_style_text_color(net_lbl[i], col, 0);
	}
}

void ui_set_steps(uint32_t steps)
{
	ui_steps_set(steps);
}

void ui_set_today(uint32_t steps)
{
	ui_home_set_today(steps);
	ui_steps_set_today(steps);
}

void ui_set_dyn(uint16_t dyn_mg)
{
	if(page == UI_PAGE_CALIB) ui_calib_set(dyn_mg, calib_th, calib_gap);
}

void ui_show_alert(uint8_t kind)
{
	ui_alert_set(kind);
	if(page == UI_PAGE_ALERT) return;
	page = UI_PAGE_ALERT;
	lv_scr_load(scr_alert);
}

void ui_set_weather(const char *line)
{
	ui_weather_set(line);
}

void ui_set_voice(const char *text)
{
	ui_voice_set(text);
}

static void leave_calib(void)
{
	uint16_t th = ui_calib_threshold();
	uint16_t gap = ui_calib_gap();

	if(th != calib_th || gap != calib_gap)
	{
		calib_th = th;
		calib_gap = gap;
		calib_dirty = 1;
		if(save_cb) save_cb();
	}
	page = UI_PAGE_SUB;
	lv_scr_load_anim(scr_sub[1], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}

uint8_t ui_on_voice(void)
{
	return page == UI_PAGE_SUB && ui_home_focus() == 3;
}

void ui_on_key(uint8_t key)
{
	uint8_t focus;

	if(page == UI_PAGE_ALERT)
	{
		/* 任意键清掉告警，回主页。 */
		page = UI_PAGE_HOME;
		lv_scr_load(scr_home);
		return;
	}

	if(page == UI_PAGE_CALIB)
	{
		if(key == KEY_BACK) leave_calib();
		else
		{
			ui_calib_on_key(key);
			calib_th = ui_calib_threshold();
			calib_gap = ui_calib_gap();
		}
		return;
	}

	if(page == UI_PAGE_SETTIME)
	{
		if(key == KEY_BACK)
		{
			if(time_cb) time_cb(ui_settime_hour(), ui_settime_min(), ui_settime_sec());
			calib_dirty = 4;
			if(save_cb) save_cb();
			page = UI_PAGE_SUB;
			lv_scr_load_anim(scr_sub[0], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
		}
		else ui_settime_on_key(key);
		return;
	}

	if(page == UI_PAGE_TIMER)
	{
		if(key == KEY_BACK)
		{
			page = UI_PAGE_SUB;
			lv_scr_load_anim(scr_sub[0], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
		}
		else ui_timer_on_key(key);
		return;
	}

	if(page == UI_PAGE_CALC)
	{
		if(key == KEY_BACK_LONG)
		{
			page = UI_PAGE_HOME;
			lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
		}
		else ui_calc_on_key(key);
		return;
	}

	if(page == UI_PAGE_GAMES)
	{
		if(key == KEY_BACK)
		{
			page = UI_PAGE_HOME;
			lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
		}
		else if(key == KEY_OK)
		{
			lv_obj_t *dest;
			uint8_t which = ui_games_focus();
			/* 三个游戏的格子很多，开机时一起建会撑满 LVGL 的内存池，所以进哪个建哪个。 */
			if(which == 1) dest = ui_pong_ensure();
			else if(which == 2) dest = ui_tetris_ensure();
			else dest = ui_snake_ensure();
			page = (uint8_t)(UI_PAGE_SNAKE + which);
			lv_scr_load_anim(dest, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
			ui_game_start(which);
		}
		else ui_games_on_key(key);
		return;
	}

	if(page == UI_PAGE_SNAKE || page == UI_PAGE_PONG || page == UI_PAGE_TETRIS)
	{
		/* 贪吃蛇和俄罗斯方块的左键是转向，长按才退出。乒乓球短按即退出。 */
		uint8_t leave = 0;
		if(page == UI_PAGE_PONG && key == KEY_BACK) leave = 1;
		if((page == UI_PAGE_SNAKE || page == UI_PAGE_TETRIS) && key == KEY_BACK_LONG) leave = 1;
		if(leave)
		{
			uint8_t was = page;
			page = UI_PAGE_GAMES;
			lv_scr_load_anim(scr_games, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
			ui_game_stop();
			if(was == UI_PAGE_SNAKE) ui_snake_stop();
			else if(was == UI_PAGE_PONG) ui_pong_stop();
			else ui_tetris_stop();
		}
		else if(page == UI_PAGE_SNAKE) ui_snake_on_key(key);
		else if(page == UI_PAGE_PONG) ui_pong_on_key(key);
		else ui_tetris_on_key(key);
		return;
	}

	if(page == UI_PAGE_HOME) {
		focus = ui_home_focus();
		if(key == KEY_DOWN) ui_home_set_focus((uint8_t)((focus + 1u) % 6u));
		else if(key == KEY_UP) ui_home_set_focus((uint8_t)((focus + 5u) % 6u));
		else if(key == KEY_OK) {
			if(focus == 4) {
				page = UI_PAGE_CALC;
				lv_scr_load_anim(scr_calc, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
			} else if(focus == 5) {
				page = UI_PAGE_GAMES;
				if(!scr_games) scr_games = ui_games_create();
				lv_scr_load_anim(scr_games, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
			} else {
			page = UI_PAGE_SUB;
			lv_scr_load_anim(scr_sub[focus], LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
			if(focus == 2) ui_weather_on_enter();
			}	/* 进天气页请求一次 */
		}
	} else if(key == KEY_BACK) {
		page = UI_PAGE_HOME;
		lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
	} else if(ui_home_focus() == 0 && key == KEY_OK) {
		ui_settime_begin(cur_h, cur_m, cur_s);
		page = UI_PAGE_SETTIME;
		lv_scr_load_anim(scr_settime, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
	} else if(ui_home_focus() == 0 && key == KEY_DOWN) {
		page = UI_PAGE_TIMER;
		lv_scr_load_anim(scr_timer, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
	} else if(ui_home_focus() == 1 && key == KEY_OK) {
		/* 计步页清零：步数置 0 会由传感器任务下一拍发现并保存。 */
		ui_steps_set(0);
		ui_set_today(0);
		calib_dirty = 2;
		if(save_cb) save_cb();
	} else if(ui_home_focus() == 1 && key == KEY_DOWN) {
		ui_calib_set(0xFFFF, calib_th, calib_gap);
		page = UI_PAGE_CALIB;
		lv_scr_load_anim(scr_calib, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
	} else if(ui_home_focus() == 2) {
		ui_weather_on_key(key);	/* 天气页：右键切换城市 */
	}
}

void ui_init(void)
{
	clock_n=temp_n=net_n=0;
	online_state=0xFF;	/* 强制第一次刷新 */
	calib_th = 150;
	calib_gap = 300;
	calib_dirty = 0;

	scr_home = ui_home_create();
	scr_sub[0] = ui_time_create();
	scr_sub[1] = ui_steps_create();
	scr_sub[2] = ui_weather_create();
	scr_sub[3] = ui_voice_create();
	scr_calib = ui_calib_create();
	scr_settime = ui_settime_create();
	scr_alert = ui_alert_create();
	scr_timer = ui_timer_create();
	scr_calc = ui_calc_create();
	page = UI_PAGE_HOME;
	lv_scr_load(scr_home);
}

void ui_calib_params(uint16_t *th_mg, uint16_t *gap_ms)
{
	*th_mg = calib_th;
	*gap_ms = calib_gap;
}

void ui_set_calib(uint16_t th_mg, uint16_t gap_ms)
{
	calib_th = th_mg;
	calib_gap = gap_ms;
}

/* 返回并清掉保存请求：0 无，1 改了阈值，2 清零。 */
uint8_t ui_take_save(void)
{
	uint8_t d = calib_dirty;
	calib_dirty = 0;
	return d;
}
