#include "ui_calib.h"
#include "ui_internal.h"
#include "pedometer.h"
#include "key.h"

static lv_obj_t *lbl_dyn;
static lv_obj_t *lbl_th;
static lv_obj_t *lbl_gap;
static uint16_t th_mg;
static uint16_t gap_ms;
static uint16_t dyn_shown;

static void paint(void)
{
	if(lbl_th) lv_label_set_text_fmt(lbl_th, "%u.%02u g", th_mg / 1000u, (th_mg % 1000u) / 10u);
	if(lbl_gap) lv_label_set_text_fmt(lbl_gap, "%u ms", gap_ms);
}

lv_obj_t *ui_calib_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	lv_obj_t *t1;
	lv_obj_t *t2;
	lv_obj_t *t3;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 292, 300);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 6);

	t1 = lv_label_create(card);
	lv_obj_set_style_text_font(t1, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(t1, UI_MUTED, 0);
	lv_label_set_text(t1, "\xE5\x8A\xA8\xE6\x80\x81\xE9\x87\x8F");	/* 动态量 */
	lv_obj_align(t1, LV_ALIGN_TOP_MID, 0, 24);

	lbl_dyn = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_dyn, &lv_font_montserrat_48, 0);
	lv_obj_set_style_text_color(lbl_dyn, UI_ACCENT, 0);
	lv_label_set_text(lbl_dyn, "0.00");
	lv_obj_align(lbl_dyn, LV_ALIGN_TOP_MID, 0, 52);

	t2 = lv_label_create(card);
	lv_obj_set_style_text_font(t2, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(t2, UI_MUTED, 0);
	lv_label_set_text(t2, "\xE9\x98\x88\xE5\x80\xBC");	/* 阈值 */
	lv_obj_align(t2, LV_ALIGN_TOP_LEFT, 28, 150);

	lbl_th = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_th, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_th, UI_FG, 0);
	lv_obj_align(lbl_th, LV_ALIGN_TOP_RIGHT, -28, 150);

	t3 = lv_label_create(card);
	lv_obj_set_style_text_font(t3, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(t3, UI_MUTED, 0);
	lv_label_set_text(t3, "\xE6\xAD\xA5\xE9\x97\xB4\xE9\x9A\x94");	/* 步间隔 */
	lv_obj_align(t3, LV_ALIGN_TOP_LEFT, 28, 196);

	lbl_gap = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_gap, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_gap, UI_FG, 0);
	lv_obj_align(lbl_gap, LV_ALIGN_TOP_RIGHT, -28, 196);

	th_mg = 150;
	gap_ms = 300;
	dyn_shown = 0xFFFF;
	paint();

	/* 上加阈值  下减阈值  右键步间隔  左键返回 */
	ui_footer_create(scr, LV_SYMBOL_UP "+ " LV_SYMBOL_DOWN "-  " LV_SYMBOL_RIGHT " \xE9\x97\xB4\xE9\x9A\x94  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_calib_set(uint16_t dyn_mg, uint16_t th, uint16_t gap)
{
	th_mg = th;
	gap_ms = gap;
	paint();
	if(dyn_mg == 0xFFFF || dyn_mg == dyn_shown) return;
	dyn_shown = dyn_mg;
	if(lbl_dyn) lv_label_set_text_fmt(lbl_dyn, "%u.%02u", dyn_mg / 1000u, (dyn_mg % 1000u) / 10u);
}

void ui_calib_on_key(uint8_t key)
{
	if(key == KEY_UP)
	{
		if(th_mg + PEDO_TH_STEP <= PEDO_TH_MAX) th_mg = (uint16_t)(th_mg + PEDO_TH_STEP);
	}
	else if(key == KEY_DOWN)
	{
		if(th_mg >= PEDO_TH_MIN + PEDO_TH_STEP) th_mg = (uint16_t)(th_mg - PEDO_TH_STEP);
	}
	else if(key == KEY_OK)
	{
		gap_ms = Pedometer_NextGap(gap_ms);
	}
	else
	{
		return;
	}
	paint();
}

uint16_t ui_calib_threshold(void)
{
	return th_mg;
}

uint16_t ui_calib_gap(void)
{
	return gap_ms;
}
