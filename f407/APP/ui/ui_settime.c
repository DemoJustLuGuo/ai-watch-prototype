#include "ui_settime.h"
#include "ui_internal.h"
#include "key.h"

static lv_obj_t *lbl[3];
static lv_obj_t *lbl_alarm;
static uint8_t val[3];
static uint8_t field;		/* 0 时  1 分  2 秒  3 闹钟开关  4 闹钟时  5 闹钟分 */
static uint8_t alarm_on;
static uint8_t alarm_h, alarm_m;

static void paint_one(uint8_t i)
{
	if(!lbl[i]) return;
	lv_label_set_text_fmt(lbl[i], "%02u", val[i]);
	lv_obj_set_style_text_color(lbl[i], i == field ? UI_ACCENT : UI_FG, 0);
}

static void paint_alarm(void)
{
	if(!lbl_alarm) return;
	if(alarm_on)
		lv_label_set_text_fmt(lbl_alarm, "\xE9\x97\xB9\xE9\x92\x9F %02u:%02u", alarm_h, alarm_m);
	else
		lv_label_set_text(lbl_alarm, "\xE9\x97\xB9\xE9\x92\x9F \xE5\x85\xB3");	/* 闹钟 关 */
	lv_obj_set_style_text_color(lbl_alarm, field >= 3 ? UI_ACCENT : UI_MUTED, 0);
}

lv_obj_t *ui_settime_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *colon1;
	lv_obj_t *colon2;
	uint8_t i;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 300, 180);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	for(i = 0; i < 3; i++)
	{
		lbl[i] = lv_label_create(card);
		lv_obj_set_style_text_font(lbl[i], &lv_font_montserrat_48, 0);
		lv_obj_align(lbl[i], LV_ALIGN_CENTER, (lv_coord_t)((int)i * 96 - 96), 0);
		val[i] = 0;
		paint_one(i);
	}

	colon1 = lv_label_create(card);
	lv_obj_set_style_text_font(colon1, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(colon1, UI_MUTED, 0);
	lv_label_set_text(colon1, ":");
	lv_obj_align(colon1, LV_ALIGN_CENTER, -48, -4);

	colon2 = lv_label_create(card);
	lv_obj_set_style_text_font(colon2, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(colon2, UI_MUTED, 0);
	lv_label_set_text(colon2, ":");
	lv_obj_align(colon2, LV_ALIGN_CENTER, 48, -4);

	lbl_alarm = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_alarm, &lv_font_chinese_16, 0);
	lv_obj_align(lbl_alarm, LV_ALIGN_BOTTOM_MID, 0, -16);
	alarm_on = 0;
	alarm_h = 7;
	alarm_m = 0;

	/* 上加 下减  右键切换字段  左键确认 */
	ui_footer_create(scr, LV_SYMBOL_UP "+ " LV_SYMBOL_DOWN "-  " LV_SYMBOL_RIGHT " \xE5\x88\x87\xE6\x8D\xA2  " LV_SYMBOL_LEFT " \xE7\xA1\xAE\xE8\xAE\xA4");
	return scr;
}

void ui_settime_begin(uint8_t h, uint8_t m, uint8_t s)
{
	val[0] = h;
	val[1] = m;
	val[2] = s;
	field = 0;
	paint_one(0);
	paint_one(1);
	paint_one(2);
	paint_alarm();
}

void ui_settime_on_key(uint8_t key)
{
	if(key == KEY_OK)
	{
		uint8_t prev = field;
		field = (uint8_t)((field + 1u) % 6u);
		if(prev < 3) paint_one(prev);
		if(field < 3) paint_one(field);
		paint_alarm();
		return;
	}
	if(key != KEY_UP && key != KEY_DOWN) return;

	if(field < 3)
	{
		uint8_t limit = field == 0 ? 24u : 60u;
		if(key == KEY_UP) val[field] = (uint8_t)((val[field] + 1u) % limit);
		else val[field] = (uint8_t)((val[field] + limit - 1u) % limit);
		paint_one(field);
	}
	else if(field == 3)
	{
		alarm_on = !alarm_on;
		paint_alarm();
	}
	else
	{
		uint8_t *p = field == 4 ? &alarm_h : &alarm_m;
		uint8_t limit = field == 4 ? 24u : 60u;
		if(key == KEY_UP) *p = (uint8_t)((*p + 1u) % limit);
		else *p = (uint8_t)((*p + limit - 1u) % limit);
		paint_alarm();
	}
}

uint8_t ui_settime_hour(void) { return val[0]; }
uint8_t ui_settime_min(void)  { return val[1]; }
uint8_t ui_settime_sec(void)  { return val[2]; }

void ui_alarm_set(uint8_t enabled, uint8_t h, uint8_t m)
{
	alarm_on = enabled ? 1u : 0u;
	alarm_h = h;
	alarm_m = m;
	paint_alarm();
}

uint8_t ui_alarm_enabled(void) { return alarm_on; }
uint8_t ui_alarm_hour(void) { return alarm_h; }
uint8_t ui_alarm_min(void) { return alarm_m; }
