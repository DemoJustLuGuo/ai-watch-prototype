#include "ui_steps.h"
#include "ui_internal.h"

#define STEPS_GOAL 10000u

static lv_obj_t *lbl_value;
static lv_obj_t *lbl_today;
static lv_obj_t *bar;
static uint32_t shown;

static void anim_steps(void *var, int32_t v)
{
	(void)var;
	if(lbl_value) lv_label_set_text_fmt(lbl_value, "%d", (int)v);
	if(bar) lv_bar_set_value(bar, v > (int32_t)STEPS_GOAL ? (int32_t)STEPS_GOAL : v, LV_ANIM_OFF);
}

lv_obj_t *ui_steps_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *title;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 280, 220);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	title = lv_label_create(card);
	lv_obj_set_style_text_font(title, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(title, UI_MUTED, 0);
	lv_label_set_text(title, "\xE8\xAE\xA1\xE6\xAD\xA5");	/* 计步 */
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

	lbl_value = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(lbl_value, UI_ACCENT, 0);
	lv_label_set_text(lbl_value, "0");
	lv_obj_align(lbl_value, LV_ALIGN_CENTER, 0, -4);

	bar = lv_bar_create(card);
	lv_obj_set_size(bar, 200, 10);
	lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -56);
	lv_bar_set_range(bar, 0, (int32_t)STEPS_GOAL);
	lv_bar_set_value(bar, 0, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(bar, UI_LINE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(bar, UI_ACCENT, LV_PART_INDICATOR);
	lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
	lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);

	lbl_today = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_today, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_today, UI_MUTED, 0);
	lv_label_set_text(lbl_today, "\xE4\xBB\x8A\xE6\x97\xA5 0");
	lv_obj_align(lbl_today, LV_ALIGN_BOTTOM_MID, 0, -24);
	ui_footer_create(scr, LV_SYMBOL_RIGHT " \xE6\xB8\x85\xE9\x9B\xB6  " LV_SYMBOL_DOWN " \xE6\xA0\xA1\xE5\x87\x86  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_steps_set(uint32_t steps)
{
	lv_anim_t a;

	if(steps == shown) return;
	lv_anim_del(lbl_value, anim_steps);
	lv_anim_init(&a);
	lv_anim_set_var(&a, lbl_value);
	lv_anim_set_exec_cb(&a, anim_steps);
	lv_anim_set_values(&a, (int32_t)shown, (int32_t)steps);
	lv_anim_set_time(&a, 300);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);
	shown = steps;
}

void ui_steps_set_today(uint32_t steps)
{
	if(lbl_today) lv_label_set_text_fmt(lbl_today, "\xE4\xBB\x8A\xE6\x97\xA5 %u", (unsigned)steps);
}
