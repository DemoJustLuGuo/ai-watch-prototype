#include "ui_voice.h"
#include "ui_internal.h"
#include <string.h>

static lv_obj_t *lbl_state;
static lv_obj_t *dot;

/* "录音" 的 UTF-8，只有录音中才呼吸。 */
static const char REC[] = "\xE5\xBD\x95\xE9\x9F\xB3";

static void anim_dot(void *obj, int32_t v)
{
	/* 用亮度而不是透明度：透明度会让 LVGL 为这个点单独开一层缓冲。 */
	lv_obj_set_style_bg_color((lv_obj_t *)obj,
		lv_color_mix(UI_ACCENT, UI_CARD, (lv_opa_t)v), 0);
}

static void breathe(uint8_t on)
{
	lv_anim_t a;

	lv_anim_del(dot, anim_dot);
	if(!on)
	{
		lv_obj_set_style_bg_color(dot, UI_ACCENT, 0);
		return;
	}
	lv_anim_init(&a);
	lv_anim_set_var(&a, dot);
	lv_anim_set_exec_cb(&a, anim_dot);
	lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
	lv_anim_set_time(&a, 800);
	lv_anim_set_playback_time(&a, 800);
	lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
	lv_anim_start(&a);
}

lv_obj_t *ui_voice_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *title;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 292, 240);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	title = lv_label_create(card);
	lv_obj_set_style_text_font(title, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(title, UI_MUTED, 0);
	lv_label_set_text(title, "AI\xE5\xAF\xB9\xE8\xAF\x9D");	/* AI 对话 */
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

	dot = lv_obj_create(card);
	lv_obj_remove_style_all(dot);
	lv_obj_set_size(dot, 18, 18);
	lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(dot, UI_ACCENT, 0);
	lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 60);

	lbl_state = lv_label_create(card);
	lv_obj_set_width(lbl_state, 260);
	lv_obj_set_style_text_font(lbl_state, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_state, UI_FG, 0);
	lv_obj_set_style_text_align(lbl_state, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(lbl_state, LV_LABEL_LONG_WRAP);
	lv_label_set_text(lbl_state, "\xE7\xA9\xBA\xE9\x97\xB2");	/* 空闲 */
	lv_obj_align(lbl_state, LV_ALIGN_CENTER, 0, 30);

	ui_footer_create(scr, "\xE9\x95\xBF\xE6\x8C\x89" LV_SYMBOL_UP " \xE8\xAF\xB4\xE8\xAF\x9D  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_voice_set(const char *text)
{
	if(!lbl_state || !text) return;
	if(strcmp(lv_label_get_text(lbl_state), text) == 0) return;	/* 每拍都调用，没变就不动 */
	lv_label_set_text(lbl_state, text);
	breathe(strcmp(text, REC) == 0);
}
