#include "ui_games.h"
#include "ui_internal.h"
#include "key.h"
#include "system.h"

/* 棋盘是一个画布，不是一格一个对象。LVGL 内存池只有 40 KB，
   几百个格子对象会在建页时耗尽。 */

#define COLS 16
#define ROWS 22
#define CELL 14

/* 8 位索引色，像素和调色板都在外部 SRAM。真彩色整张画布有 135 KB，
   每次重绘都要经 FSMC 搬到屏幕上，来回进出游戏时会把刷新拖死。 */
__attribute__((section(".bss.game"), zero_init)) static lv_color_t canvas_buf[COLS * CELL * ROWS * CELL];
static lv_obj_t *board;
static lv_obj_t *lbl_over;

static uint8_t gx[COLS * ROWS];
static uint8_t gy[COLS * ROWS];
static uint16_t len;
static uint16_t head;
static int8_t dx, dy;
static int8_t ndx, ndy;
static uint8_t fx, fy;
static uint8_t over;


static void set_board(lv_obj_t *c)
{
	board = c;
	lv_canvas_set_buffer(board, canvas_buf, COLS * CELL, ROWS * CELL, LV_IMG_CF_TRUE_COLOR);
	lv_obj_align(board, LV_ALIGN_TOP_MID, 0, 44);
}

static uint32_t rnd(void)
{
	static uint32_t s;
	if(!s) s = HAL_GetTick() | 1u;
	s = s * 1664525u + 1013904223u;
	return s;
}

/* 直接改画布缓冲，不走 lv_canvas_draw_rect。那个函数每画一格就把整张
   棋盘标成待刷新，一步三格就会把 LVGL 只有 32 项的刷新队列撑满，触发断言后卡死。 */
static void mark(lv_obj_t *canvas, uint8_t x, uint8_t y)
{
	lv_area_t a;
	lv_obj_get_coords(canvas, &a);
	a.x2 = (lv_coord_t)(a.x1 + (x + 1) * CELL - 1);
	a.y2 = (lv_coord_t)(a.y1 + (y + 1) * CELL - 1);
	a.x1 = (lv_coord_t)(a.x1 + x * CELL);
	a.y1 = (lv_coord_t)(a.y1 + y * CELL);
	lv_obj_invalidate_area(canvas, &a);
}

static void cell(uint8_t x, uint8_t y, lv_color_t c)
{
	uint16_t row, col;
	uint16_t stride = COLS * CELL;
	for(row = 1; row < CELL - 1; row++)
	{
		lv_color_t *p = canvas_buf + ((uint32_t)y * CELL + row) * stride + (uint32_t)x * CELL + 1;
		for(col = 1; col < CELL - 1; col++) *p++ = c;
	}
}

static void place_food(void)
{
	uint16_t n, i;
	fx = (uint8_t)(rnd() % COLS);
	fy = (uint8_t)(rnd() % ROWS);
	for(n = 0; n < COLS * ROWS; n++)
	{
		uint8_t hit = 0;
		for(i = 0; i < len; i++)
		{
			uint16_t k = (uint16_t)((head + COLS * ROWS - i) % (COLS * ROWS));
			if(gx[k] == fx && gy[k] == fy) { hit = 1; break; }
		}
		if(!hit) break;
		fx = (uint8_t)((fx + 1) % COLS);
		if(!fx) fy = (uint8_t)((fy + 1) % ROWS);
	}
	cell(fx, fy, UI_OFFLINE);
	mark(board, fx, fy);
}

static void new_game(void)
{
	uint16_t i;
	lv_memset_00(canvas_buf, sizeof(canvas_buf));
	len = 3;
	head = 2;
	gx[0] = 3; gy[0] = 11;
	gx[1] = 4; gy[1] = 11;
	gx[2] = 5; gy[2] = 11;
	dx = 1; dy = 0;
	ndx = 1; ndy = 0;
	over = 0;
	for(i = 0; i < len; i++) { cell(gx[i], gy[i], UI_ACCENT); mark(board, gx[i], gy[i]); }
	place_food();
	if(lbl_over) lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
}

void ui_snake_step(void)
{
	int16_t nx, ny;
	uint16_t i, nh, tail;

	if(over) return;
	dx = ndx; dy = ndy;
	nx = (int16_t)gx[head] + dx;
	ny = (int16_t)gy[head] + dy;
	if(nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) { over = 1; goto done; }
	for(i = 0; i < len; i++)
	{
		uint16_t k = (uint16_t)((head + COLS * ROWS - i) % (COLS * ROWS));
		if(gx[k] == (uint8_t)nx && gy[k] == (uint8_t)ny && i != len - 1)
		{
			over = 1;
			goto done;
		}
	}
	nh = (uint16_t)((head + 1) % (COLS * ROWS));
	gx[nh] = (uint8_t)nx;
	gy[nh] = (uint8_t)ny;
	head = nh;
	cell((uint8_t)nx, (uint8_t)ny, UI_ACCENT);
	mark(board, (uint8_t)nx, (uint8_t)ny);
	if((uint8_t)nx == fx && (uint8_t)ny == fy)
	{
		if(len < COLS * ROWS - 1) len++;
		place_food();
	}
	else
	{
		tail = (uint16_t)((head + COLS * ROWS - len) % (COLS * ROWS));
		cell(gx[tail], gy[tail], UI_BG);
		mark(board, gx[tail], gy[tail]);
	}
	return;
done:
	if(lbl_over) lv_obj_clear_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *build(void);

lv_obj_t *ui_snake_create(void)
{
	return NULL;
}

lv_obj_t *ui_snake_ensure(void)
{
	static lv_obj_t *scr;
	if(!scr) scr = build();
	else if(!board)
	{
		board = lv_canvas_create(scr);
		set_board(board);
		new_game();
	}
	return scr;
}



static lv_obj_t *build(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	ui_style_screen(scr);
	ui_status_create(scr);

	board = lv_canvas_create(scr);
	set_board(board);

	lbl_over = lv_label_create(scr);
	lv_obj_set_style_text_font(lbl_over, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_over, UI_OFFLINE, 0);
	lv_label_set_text(lbl_over, "\xE7\xBB\x93\xE6\x9D\x9F  \xE5\x86\x8D\xE6\x9D\xA5");
	lv_obj_align(lbl_over, LV_ALIGN_BOTTOM_MID, 0, -40);
	lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);

	ui_footer_create(scr, LV_SYMBOL_UP LV_SYMBOL_DOWN LV_SYMBOL_LEFT LV_SYMBOL_RIGHT
		" \xE8\xBD\xAC\xE5\x90\x91 \xE9\x95\xBF\xE6\x8C\x89" LV_SYMBOL_LEFT " \xE9\x80\x80\xE5\x87\xBA");
	new_game();
	return scr;
}

void ui_snake_on_key(uint8_t key)
{
	if(over)
	{
		if(key == KEY_OK) new_game();
		return;
	}
	if(key == KEY_UP && dy == 0) { ndx = 0; ndy = -1; }
	else if(key == KEY_DOWN && dy == 0) { ndx = 0; ndy = 1; }
	else if(key == KEY_BACK && dx == 0) { ndx = -1; ndy = 0; }
	else if(key == KEY_OK && dx == 0) { ndx = 1; ndy = 0; }
}

void ui_snake_stop(void)
{
	if(board) { lv_obj_del(board); board = NULL; }
	lbl_over = NULL;
	over = 0;
}


