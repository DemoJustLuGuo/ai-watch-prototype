#include "ui_alert.h"
#include "ui_internal.h"

static lv_obj_t *title;

lv_obj_t *ui_alert_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *mark;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 292, 220);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	mark = lv_label_create(card);
	lv_obj_set_style_text_font(mark, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(mark, lv_color_hex(0xFF5A5A), 0);
	lv_label_set_text(mark, "!");
	lv_obj_align(mark, LV_ALIGN_CENTER, 0, -36);

	title = lv_label_create(card);
	lv_obj_set_style_text_font(title, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(title, UI_FG, 0);
	lv_label_set_text(title, "\xE6\x8F\x90\xE9\x86\x92");	/* 提醒 */
	lv_obj_align(title, LV_ALIGN_CENTER, 0, 28);

	/* 任意键返回 */
	ui_footer_create(scr, "\xE6\x8C\x89\xE9\x94\xAE\xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_alert_set(uint8_t kind)
{
	const char *txt = "\xE6\xA3\x80\xE6\xB5\x8B\xE5\x88\xB0\xE8\xB7\x8C\xE5\x80\x92";	/* 检测到跌倒 */
	if(kind == 1) txt = "\xE5\x80\x92\xE8\xAE\xA1\xE6\x97\xB6\xE5\x88\xB0\xE7\x82\xB9";	/* 倒计时到点 */
	else if(kind == 2) txt = "\xE9\x97\xB9\xE9\x92\x9F\xE5\x88\xB0\xE7\x82\xB9";		/* 闹钟到点 */
	if(title) lv_label_set_text(title, txt);
}
