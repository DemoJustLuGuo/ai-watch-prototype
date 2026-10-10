#include "ui_calc.h"
#include "ui_internal.h"
#include "key.h"

/* 键位 4 列 5 行：数字、小数点、四则、等号、清除。 */
static const char *const keys[20] = {
	"7", "8", "9", "/",
	"4", "5", "6", "*",
	"1", "2", "3", "-",
	"0", ".", "=", "+",
	"C", "",  "",  "",
};

static lv_obj_t *btns[20];
static lv_obj_t *lbl_expr;
static uint8_t cursor;

static char entry[16];		/* 正在输入的数 */
static uint8_t entry_len;
static double acc;		/* 上一次结果 */
static char op;			/* 待运算，0 表示没有 */
static uint8_t fresh;		/* 刚按过运算符或等号，下一次数字要覆盖 */

static void paint_cursor(void)
{
	uint8_t i;
	for(i = 0; i < 20; i++)
	{
		if(!btns[i]) continue;
		lv_obj_set_style_bg_color(btns[i], i == cursor ? UI_ACCENT : UI_LINE, 0);
	}
}

static void paint_expr(void)
{
	if(!lbl_expr) return;
	if(entry_len) lv_label_set_text(lbl_expr, entry);
	else lv_label_set_text_fmt(lbl_expr, "%.6g", acc);
}

static void entry_clear(void)
{
	entry_len = 0;
	entry[0] = 0;
}

static double entry_value(void)
{
	double v = 0, frac = 0, base = 1;
	uint8_t dot = 0, i;
	for(i = 0; i < entry_len; i++)
	{
		if(entry[i] == '.') { dot = 1; continue; }
		if(!dot) v = v * 10 + (entry[i] - '0');
		else { base *= 10; frac = frac * 10 + (entry[i] - '0'); }
	}
	return v + frac / base;
}

static void apply_op(void)
{
	double v = entry_len ? entry_value() : acc;
	if(op == '+') acc += v;
	else if(op == '-') acc -= v;
	else if(op == '*') acc *= v;
	else if(op == '/' && v != 0) acc /= v;
	else acc = v;
	op = 0;
	entry_clear();
}

static void press_key(const char *k)
{
	char c = k[0];
	if(c == 0) return;
	if(c == 'C')
	{
		acc = 0; op = 0; fresh = 0;
		entry_clear();
	}
	else if(c >= '0' && c <= '9')
	{
		if(fresh) { entry_clear(); fresh = 0; }
		if(entry_len < sizeof(entry) - 1)
		{
			entry[entry_len++] = c;
			entry[entry_len] = 0;
		}
	}
	else if(c == '.')
	{
		uint8_t i, has = 0;
		if(fresh) { entry_clear(); fresh = 0; }
		for(i = 0; i < entry_len; i++) if(entry[i] == '.') has = 1;
		if(!has && entry_len < sizeof(entry) - 1)
		{
			if(!entry_len) entry[entry_len++] = '0';
			entry[entry_len++] = '.';
			entry[entry_len] = 0;
		}
	}
	else if(c == '=')
	{
		apply_op();
		fresh = 1;
	}
	else
	{
		apply_op();
		op = c;
		fresh = 1;
	}
	paint_expr();
}

lv_obj_t *ui_calc_create(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);
	lv_obj_t *card;
	uint8_t i;

	ui_style_screen(scr);
	ui_status_create(scr);

	card = ui_card(scr, 300, 360);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 10);

	lbl_expr = lv_label_create(card);
	lv_obj_set_style_text_font(lbl_expr, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_expr, UI_FG, 0);
	lv_obj_set_width(lbl_expr, 260);
	lv_obj_set_style_text_align(lbl_expr, LV_TEXT_ALIGN_RIGHT, 0);
	lv_label_set_text(lbl_expr, "0");
	lv_obj_align(lbl_expr, LV_ALIGN_TOP_MID, 0, 16);

	for(i = 0; i < 20; i++)
	{
		uint8_t col = i % 4, row = i / 4;
		btns[i] = lv_obj_create(card);
		lv_obj_remove_style_all(btns[i]);
		lv_obj_set_size(btns[i], 64, 52);
		lv_obj_set_pos(btns[i], (lv_coord_t)(14 + col * 70), (lv_coord_t)(52 + row * 58));
		lv_obj_set_style_radius(btns[i], 8, 0);
		if(keys[i][0])
		{
			lv_obj_t *t = lv_label_create(btns[i]);
			lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
			lv_obj_set_style_text_color(t, UI_FG, 0);
			lv_label_set_text(t, keys[i]);
			lv_obj_center(t);
		}
	}
	cursor = 0;
	acc = 0; op = 0; fresh = 0;
	entry_clear();
	paint_cursor();

	ui_footer_create(scr, LV_SYMBOL_UP LV_SYMBOL_DOWN LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " \xE9\x80\x89\xE6\x8B\xA9  \xE9\x95\xBF\xE6\x8C\x89" LV_SYMBOL_RIGHT " \xE7\xA1\xAE\xE8\xAE\xA4  \xE9\x95\xBF\xE6\x8C\x89" LV_SYMBOL_LEFT " \xE9\x80\x80\xE5\x87\xBA");
	return scr;
}

static uint8_t cell_filled(uint8_t i)
{
	return (i < 20 && keys[i][0]) ? 1u : 0u;
}

/* 从当前格沿方向找下一格，末行的空格跳过。找到返回 1。 */
static uint8_t move_cursor(int8_t dc, int8_t dr)
{
	int8_t col = (int8_t)(cursor % 4);
	int8_t row = (int8_t)(cursor / 4);
	uint8_t guard;

	for(guard = 0; guard < 5; guard++)
	{
		col = (int8_t)(col + dc);
		row = (int8_t)(row + dr);
		if(col < 0 || col > 3 || row < 0 || row > 4) return 0;
		if(cell_filled((uint8_t)(row * 4 + col)))
		{
			cursor = (uint8_t)(row * 4 + col);
			return 1;
		}
	}
	return 0;
}

void ui_calc_on_key(uint8_t key)
{
	if(key == KEY_UP) move_cursor(0, -1);
	else if(key == KEY_DOWN) move_cursor(0, 1);
	else if(key == KEY_BACK) move_cursor(-1, 0);
	else if(key == KEY_OK) move_cursor(1, 0);
	else if(key == KEY_OK_LONG) press_key(keys[cursor]);
	paint_cursor();
}
