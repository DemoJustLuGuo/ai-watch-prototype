#include "ui_games.h"
#include "ui_internal.h"
#include "key.h"

static const char *const names[3] = {
	"\xE8\xB4\xAA\xE5\x90\x83\xE8\x9B\x87",		/* 贪吃蛇 */
	"\xE4\xB9\x92\xE4\xB9\x93\xE7\x90\x83",		/* 乒乓球 */
	"\xE4\xBF\x84\xE7\xBD\x97\xE6\x96\xAF\xE6\x96\xB9\xE5\x9D\x97",	/* 俄罗斯方块 */
};

static lv_obj_t *rows[3];
static uint8_t focus_index;

static void paint(void)
{
	uint8_t i;
	for(i = 0; i < 3; i++)
	{
		lv_obj_set_style_border_color(rows[i], i == focus_index ? UI_ACCENT : UI_ACCENT_DIM, 0);
		lv_obj_set_style_border_width(rows[i], i == focus_index ? 2 : 0, 0);
		lv_obj_set_style_bg_color(rows[i],
			lv_color_mix(UI_ACCENT, UI_CARD, i == focus_index ? 40 : 0), 0);
	}
}

lv_obj_t *ui_games_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	uint8_t i;

	ui_style_screen(scr);
	ui_status_create(scr);

	for(i = 0; i < 3; i++)
	{
		lv_obj_t *t;
		rows[i] = ui_card(scr, 280, 64);
		lv_obj_set_pos(rows[i], 20, (lv_coord_t)(80 + i * 84));
		t = lv_label_create(rows[i]);
		lv_obj_set_style_text_font(t, &lv_font_chinese_16, 0);
		lv_obj_set_style_text_color(t, UI_FG, 0);
		lv_label_set_text(t, names[i]);
		lv_obj_center(t);
	}
	focus_index = 0;
	paint();
	ui_footer_create(scr, LV_SYMBOL_UP LV_SYMBOL_DOWN " \xE9\x80\x89\xE6\x8B\xA9  " LV_SYMBOL_RIGHT " \xE8\xBF\x9B\xE5\x85\xA5  " LV_SYMBOL_LEFT " \xE8\xBF\x94\xE5\x9B\x9E");
	return scr;
}

void ui_games_on_key(uint8_t key)
{
	if(key == KEY_UP) focus_index = (uint8_t)((focus_index + 2u) % 3u);
	else if(key == KEY_DOWN) focus_index = (uint8_t)((focus_index + 1u) % 3u);
	else return;
	paint();
}

uint8_t ui_games_focus(void)
{
	return focus_index;
}
