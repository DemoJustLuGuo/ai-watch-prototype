#include "ui_time.h"
#include "ui_internal.h"

static lv_obj_t *lbl_hm;
static lv_obj_t *lbl_sec;
static lv_obj_t *arc;
static int32_t sec_shown = -1;

static int32_t parse_ss(const char *ss)
{
	if(!ss || ss[0] < '0' || ss[0] > '9' || ss[1] < '0' || ss[1] > '9') return 0;
	return (ss[0] - '0') * 10 + (ss[1] - '0');
}

/* 弧的指示值走 0..600，每秒占 10，这样 400 ms 的补间才有中间帧。 */
static void anim_sec(void *obj, int32_t v)
{
	lv_arc_set_value((lv_obj_t *)obj, (int16_t)(v / 10));
}

lv_obj_t *ui_time_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	ui_style_screen(scr);
	ui_status_create(scr);

	arc = lv_arc_create(scr);
	lv_obj_set_size(arc, 220, 220);
	lv_obj_align(arc, LV_ALIGN_CENTER, 0, 10);
	lv_arc_set_range(arc, 0, 60);
	lv_arc_set_bg_angles(arc, 0, 360);
	lv_arc_set_rotation(arc, 270);		/* 0 秒从正上方起 */
	lv_arc_set_value(arc, 0);
	lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
	lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(arc, UI_LINE, LV_PART_MAIN);
	lv_obj_set_style_arc_color(arc, UI_ACCENT, LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);

	lbl_hm = lv_label_create(scr);
	lv_obj_set_style_text_font(lbl_hm, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(lbl_hm, UI_FG, 0);
	lv_label_set_text(lbl_hm, "12:00");
	lv_obj_align(lbl_hm, LV_ALIGN_CENTER, 0, -8);

	lbl_sec = lv_label_create(scr);
	lv_obj_set_style_text_font(lbl_sec, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_sec, UI_ACCENT, 0);
	lv_label_set_text(lbl_sec, "00");
	lv_obj_align(lbl_sec, LV_ALIGN_CENTER, 0, 30);

	ui_footer_create(scr, LV_SYMBOL_RIGHT " \xE5\xAF\xB9\xE6\x97\xB6  " LV_SYMBOL_DOWN " \xE7\xA7\x92\xE8\xA1\xA8  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_time_set(const char *hhmm, const char *ss)
{
	int32_t sec = parse_ss(ss);
	lv_anim_t a;

	if(lbl_hm) lv_label_set_text(lbl_hm, hhmm);
	if(lbl_sec) lv_label_set_text(lbl_sec, ss);
	if(!arc || sec == sec_shown) return;
	lv_anim_del(arc, anim_sec);
	/* 59 回到 0 时直接跳回起点，不让指示器倒转一圈 */
	if(sec < sec_shown || sec_shown < 0)
	{
		lv_arc_set_value(arc, (int16_t)sec);
	}
	else
	{
		lv_anim_init(&a);
		lv_anim_set_var(&a, arc);
		lv_anim_set_exec_cb(&a, anim_sec);
		lv_anim_set_values(&a, sec_shown * 10, sec * 10);
		lv_anim_set_time(&a, 400);
		lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
		lv_anim_start(&a);
	}
	sec_shown = sec;
}
