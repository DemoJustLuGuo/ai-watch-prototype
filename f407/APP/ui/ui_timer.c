#include "ui_timer.h"
#include "ui_internal.h"
#include "key.h"

#define COUNTDOWN_MAX_MIN 60u

static lv_obj_t *lbl_mode;
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_hint;
static uint8_t mode;
static uint8_t running;
static uint32_t shown_ms;
static uint8_t set_min;		/* 倒计时设定的分钟数 */

static void paint_mode(void)
{
	if(!lbl_mode) return;
	if(mode == TIMER_MODE_STOPWATCH)
		lv_label_set_text(lbl_mode, "\xE7\xA7\x92\xE8\xA1\xA8");		/* 秒表 */
	else
		lv_label_set_text(lbl_mode, "\xE5\x80\x92\xE8\xAE\xA1\xE6\x97\xB6");	/* 倒计时 */
}

static void paint_time(uint32_t ms)
{
	uint32_t total_s = ms / 1000u;
	uint32_t m = total_s / 60u;
	uint32_t s = total_s % 60u;
	uint32_t cs = (ms % 1000u) / 10u;

	if(lbl_time) lv_label_set_text_fmt(lbl_time, "%02u:%02u.%02u", (unsigned)m, (unsigned)s, (unsigned)cs);
}

static void paint_hint(void)
{
	if(!lbl_hint) return;
	if(mode == TIMER_MODE_COUNTDOWN && !running)
		lv_label_set_text_fmt(lbl_hint, LV_SYMBOL_UP "+  %u min", set_min);
	else
		lv_label_set_text(lbl_hint, running ? LV_SYMBOL_RIGHT " \xE5\x81\x9C\xE6\xAD\xA2" : LV_SYMBOL_RIGHT " \xE5\xBC\x80\xE5\xA7\x8B");
}

lv_obj_t *ui_timer_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 292, 240);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	lbl_mode = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_mode, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_mode, UI_MUTED, 0);
	lv_obj_align(lbl_mode, LV_ALIGN_TOP_MID, 0, 28);

	lbl_time = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(lbl_time, UI_ACCENT, 0);
	lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, 4);

	lbl_hint = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_hint, UI_MUTED, 0);
	lv_obj_align(lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -24);

	mode = TIMER_MODE_STOPWATCH;
	running = 0;
	shown_ms = 0;
	set_min = 1;
	paint_mode();
	paint_time(0);
	paint_hint();

	/* 下键切换秒表/倒计时  右键开始/停止  左键返回 */
	ui_footer_create(scr, LV_SYMBOL_DOWN " \xE5\x88\x87\xE6\x8D\xA2  " LV_SYMBOL_RIGHT " \xE5\xBC\x80\xE5\xA7\x8B  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_timer_on_key(uint8_t key)
{
	if(key == KEY_DOWN && !running)
	{
		/* 切换模式，同时清掉上一次的计时。 */
		mode = (mode == TIMER_MODE_STOPWATCH) ? TIMER_MODE_COUNTDOWN : TIMER_MODE_STOPWATCH;
		shown_ms = (mode == TIMER_MODE_COUNTDOWN) ? (uint32_t)set_min * 60000u : 0;
		paint_mode();
		paint_time(shown_ms);
		paint_hint();
	}
	else if(key == KEY_OK)
	{
		running = !running;
		if(running && mode == TIMER_MODE_COUNTDOWN)
			shown_ms = (uint32_t)set_min * 60000u;
		paint_hint();
	}
	else if(!running && mode == TIMER_MODE_COUNTDOWN && key == KEY_UP)
	{
		if(set_min < COUNTDOWN_MAX_MIN) set_min++;
		shown_ms = (uint32_t)set_min * 60000u;
		paint_time(shown_ms);
		paint_hint();
	}
}

uint8_t ui_timer_running(void) { return running; }
uint8_t ui_timer_mode(void) { return mode; }

void ui_timer_set(uint32_t ms)
{
	if(mode == TIMER_MODE_COUNTDOWN && ms == 0 && running)
	{
		running = 0;		/* 到点自己停下，蜂鸣由 main 发 */
		paint_hint();
	}
	shown_ms = ms;
	paint_time(ms);
}
