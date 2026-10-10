#include "ui_weather.h"
#include "ui_internal.h"
#include "key.h"

/* 城市表。UTF-8 字面量，发给 C3 时按字节长度取，不带结尾 0。 */
static const char *const cities[] = {
	"\xE9\x87\x8D\xE5\xBA\x86",	/* 重庆 */
	"\xE5\x8C\x97\xE4\xBA\xAC",	/* 北京 */
	"\xE4\xB8\x8A\xE6\xB5\xB7",	/* 上海 */
	"\xE5\xB9\xBF\xE5\xB7\x9E",	/* 广州 */
	"\xE6\x88\x90\xE9\x83\xBD",	/* 成都 */
};
#define CITY_N (sizeof(cities)/sizeof(cities[0]))

static lv_obj_t *lbl_city;
static lv_obj_t *lbl_line;
static ui_weather_req_cb_t req_cb;
static uint8_t city_idx;

static void request_current(void)
{
	if(req_cb) req_cb(cities[city_idx]);
}

lv_obj_t *ui_weather_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *title;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 292, 200);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	title = lv_label_create(card);
	lv_obj_set_style_text_font(title, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(title, UI_MUTED, 0);
	lv_label_set_text(title, "\xE5\xA4\xA9\xE6\xB0\x94");	/* 天气 */
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

	lbl_city = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_city, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_city, UI_ACCENT, 0);
	lv_label_set_text(lbl_city, cities[0]);
	lv_obj_align(lbl_city, LV_ALIGN_TOP_MID, 0, 56);

	lbl_line = lv_label_create(card);
	lv_obj_set_width(lbl_line, 260);
	lv_obj_set_style_text_font(lbl_line, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_line, UI_FG, 0);
	lv_obj_set_style_text_align(lbl_line, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(lbl_line, LV_LABEL_LONG_WRAP);
	lv_label_set_text(lbl_line, "\xE5\xA4\xA9\xE6\xB0\x94 --");	/* 天气 -- */
	lv_obj_align(lbl_line, LV_ALIGN_BOTTOM_MID, 0, -28);

	ui_footer_create(scr, LV_SYMBOL_RIGHT " \xE5\x88\x87\xE6\x8D\xA2  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_weather_set(const char *line)
{
	if(lbl_line && line) lv_label_set_text(lbl_line, line);
}

void ui_weather_set_req_cb(ui_weather_req_cb_t cb)
{
	req_cb = cb;
}

void ui_weather_on_enter(void)
{
	city_idx = 0;
	if(lbl_city) lv_label_set_text(lbl_city, cities[city_idx]);
	request_current();	/* 进页请求一次，C3 未连时由状态栏体现离线 */
}

void ui_weather_on_key(uint8_t key)
{
	if(key != KEY_OK) return;	/* 右键循环切换城市 */
	city_idx = (uint8_t)((city_idx + 1u) % CITY_N);
	if(lbl_city) lv_label_set_text(lbl_city, cities[city_idx]);
	if(lbl_line) lv_label_set_text(lbl_line, "\xE6\x9F\xA5\xE8\xAF\xA2\xE4\xB8\xAD");	/* 查询中 */
	request_current();
}
