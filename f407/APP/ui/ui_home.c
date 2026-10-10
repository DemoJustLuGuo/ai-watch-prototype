#include "ui_home.h"
#include "ui_internal.h"

static lv_obj_t *rows[6];
static lv_obj_t *today_lbl;
static uint8_t focus_index;

/* 选中程度 0..255：描边从暗到亮，背景从不透明卡片色过渡到半透明强调色。 */
static void apply_focus(lv_obj_t *row, int32_t level)
{
	if(level < 0) level = 0;
	if(level > 255) level = 255;
	lv_obj_set_style_border_color(row, level > 127 ? UI_ACCENT : UI_ACCENT_DIM, 0);
	lv_obj_set_style_border_width(row, (lv_coord_t)(level > 0 ? 2 : 0), 0);
	lv_obj_set_style_border_opa(row, (lv_opa_t)level, 0);
	/* 背景保持不透明，只在卡片色和强调色之间混，避免触发分层渲染。 */
	lv_obj_set_style_bg_color(row, lv_color_mix(UI_ACCENT, UI_CARD, (lv_opa_t)(level / 6)), 0);
	lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
}

static void anim_focus(void *row, int32_t v)
{
	apply_focus((lv_obj_t *)row, v);
}

static void move_focus(lv_obj_t *row, int32_t from, int32_t to)
{
	lv_anim_t a;

	lv_anim_del(row, anim_focus);
	lv_anim_init(&a);
	lv_anim_set_var(&a, row);
	lv_anim_set_exec_cb(&a, anim_focus);
	lv_anim_set_values(&a, from, to);
	lv_anim_set_time(&a, 150);
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);
}

static void add_row(lv_obj_t *parent, uint8_t index, const char *symbol, const char *name)
{
	lv_obj_t *row;
	lv_obj_t *sym;
	lv_obj_t *text;

	row = ui_card(parent, 304, 60);
	rows[index] = row;
	lv_obj_set_pos(row, 8, (lv_coord_t)(40 + index * 68));

	sym = lv_label_create(row);
	lv_obj_set_style_text_font(sym, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(sym, UI_ACCENT, 0);
	lv_label_set_text(sym, symbol);
	lv_obj_align(sym, LV_ALIGN_LEFT_MID, 20, 0);

	text = lv_label_create(row);
	lv_obj_set_style_text_font(text, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(text, UI_FG, 0);
	lv_label_set_text(text, name);
	lv_obj_align(text, LV_ALIGN_LEFT_MID, 56, 0);

		if(index == 1)
		{
			today_lbl = lv_label_create(row);
			lv_obj_set_style_text_font(today_lbl, &lv_font_montserrat_16, 0);
			lv_obj_set_style_text_color(today_lbl, UI_ACCENT, 0);
			lv_label_set_text(today_lbl, "0");
			lv_obj_align(today_lbl, LV_ALIGN_RIGHT_MID, -20, 0);
		}
	}

lv_obj_t *ui_home_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	uint8_t i;

	ui_style_screen(scr);
	ui_status_create(scr);
	add_row(scr, 0, LV_SYMBOL_LOOP, "\xE6\x97\xB6\xE9\x97\xB4");		/* 时间 */
	add_row(scr, 1, LV_SYMBOL_BARS, "\xE8\xAE\xA1\xE6\xAD\xA5");		/* 计步 */
	add_row(scr, 2, LV_SYMBOL_TINT, "\xE5\xA4\xA9\xE6\xB0\x94");		/* 天气 */
	add_row(scr, 3, LV_SYMBOL_AUDIO, "\xE8\xAF\xAD\xE9\x9F\xB3");		/* 语音 */
	add_row(scr, 4, LV_SYMBOL_PLUS, "计算");		/* 计算 */
	add_row(scr, 5, LV_SYMBOL_PLAY, "游戏");	/* 游戏 */
	ui_footer_create(scr, LV_SYMBOL_UP LV_SYMBOL_DOWN " \xE9\x80\x89\xE6\x8B\xA9  " LV_SYMBOL_RIGHT " \xE6\x89\x93\xE5\xBC\x80");
	focus_index = 0;
	for(i = 0; i < 6; i++) apply_focus(rows[i], i == 0 ? 255 : 0);
	return scr;
}

void ui_home_set_focus(uint8_t index)
{
	uint8_t prev = focus_index;

	if(index > 5) index = 0;
	if(index == prev) return;
	focus_index = index;
	move_focus(rows[prev], 255, 0);
	move_focus(rows[index], 0, 255);
}

uint8_t ui_home_focus(void)
{
	return focus_index;
}

void ui_home_set_today(uint32_t steps)
{
	if(today_lbl) lv_label_set_text_fmt(today_lbl, "%u", (unsigned)steps);
}
