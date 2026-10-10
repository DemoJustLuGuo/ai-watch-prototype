#include "ui_games.h"
#include "ui_internal.h"
#include "key.h"
#include "system.h"

/* 10 列 20 行，整张棋盘一个画布。左平移，右平移，上旋转，下加速一格。 */

#define COLS 10
#define ROWS 20
#define CELL 16

static const uint8_t shapes[7][4][4] = {
	{ {0x01,0x11,0x21,0x31}, {0x20,0x21,0x22,0x23}, {0x02,0x12,0x22,0x32}, {0x10,0x11,0x12,0x13} },
	{ {0x00,0x10,0x20,0x21}, {0x00,0x01,0x02,0x10}, {0x00,0x01,0x11,0x21}, {0x02,0x10,0x11,0x12} },
	{ {0x01,0x11,0x20,0x21}, {0x00,0x10,0x11,0x12}, {0x00,0x01,0x10,0x20}, {0x00,0x01,0x02,0x12} },
	{ {0x00,0x01,0x10,0x11}, {0x00,0x01,0x10,0x11}, {0x00,0x01,0x10,0x11}, {0x00,0x01,0x10,0x11} },
	{ {0x10,0x20,0x01,0x11}, {0x00,0x10,0x11,0x21}, {0x10,0x20,0x01,0x11}, {0x00,0x10,0x11,0x21} },
	{ {0x10,0x01,0x11,0x21}, {0x01,0x10,0x11,0x12}, {0x00,0x10,0x11,0x20}, {0x00,0x01,0x02,0x11} },
	{ {0x00,0x10,0x11,0x21}, {0x01,0x02,0x10,0x11}, {0x00,0x10,0x11,0x21}, {0x01,0x02,0x10,0x11} },
};

static const uint32_t colors[7] = {
	0x3DDC97, 0x4C8DFF, 0xE6B450, 0xE6D450, 0xE05070, 0xB06AE0, 0x50C8E0
};

/* 8 位索引色，理由同贪吃蛇：真彩色棋盘每次刷新都要搬 100 KB。 */
__attribute__((section(".bss.game"), zero_init)) static lv_color_t canvas_buf[COLS * CELL * ROWS * CELL];
static lv_obj_t *board_view;
static lv_obj_t *lbl_over;

static uint8_t board[ROWS][COLS];
static uint8_t kind, rot;
static int8_t px, py;
static uint8_t over;
static uint16_t lines;


static void set_board(lv_obj_t *c)
{
	board_view = c;
	lv_canvas_set_buffer(board_view, canvas_buf, COLS * CELL, ROWS * CELL, LV_IMG_CF_TRUE_COLOR);
	lv_obj_align(board_view, LV_ALIGN_TOP_MID, 0, 44);
}

static int8_t sx(uint8_t b) { return (int8_t)(b >> 4); }
static int8_t sy(uint8_t b) { return (int8_t)(b & 0x0F); }

/* 直接改缓冲，理由同贪吃蛇。消一行要重画上百格，每格一次整板刷新会立刻撑满队列。 */
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

static void paint_board(void)
{
	uint8_t x, y;
	lv_memset_00(canvas_buf, sizeof(canvas_buf));
	for(y = 0; y < ROWS; y++)
		for(x = 0; x < COLS; x++)
			if(board[y][x]) { cell(x, y, lv_color_hex(colors[board[y][x] - 1])); mark(board_view, x, y); }
}

static uint8_t fits(int8_t nx, int8_t ny, uint8_t nr)
{
	uint8_t i;
	for(i = 0; i < 4; i++)
	{
		int8_t x = (int8_t)(nx + sx(shapes[kind][nr][i]));
		int8_t y = (int8_t)(ny + sy(shapes[kind][nr][i]));
		if(x < 0 || x >= COLS || y >= ROWS) return 0;
		if(y >= 0 && board[y][x]) return 0;
	}
	return 1;
}

static void paint_piece(lv_color_t c)
{
	uint8_t i;
	for(i = 0; i < 4; i++)
	{
		int8_t x = (int8_t)(px + sx(shapes[kind][rot][i]));
		int8_t y = (int8_t)(py + sy(shapes[kind][rot][i]));
		if(y >= 0 && x >= 0 && x < COLS) { cell((uint8_t)x, (uint8_t)y, c); mark(board_view, (uint8_t)x, (uint8_t)y); }
	}
}

static uint32_t rnd(void)
{
	static uint32_t s;
	if(!s) s = (uint32_t)HAL_GetTick() | 1u;
	s = s * 1664525u + 1013904223u;
	return s;
}

static void spawn(void)
{
	kind = (uint8_t)(rnd() % 7u);
	rot = 0;
	px = 3; py = 0;
	if(!fits(px, py, rot))
	{
		over = 1;
		if(lbl_over) lv_obj_clear_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
		return;
	}
	paint_piece(lv_color_hex(colors[kind]));
}

static void lock_piece(void)
{
	uint8_t i, y, x;
	for(i = 0; i < 4; i++)
	{
		int8_t cx = (int8_t)(px + sx(shapes[kind][rot][i]));
		int8_t cy = (int8_t)(py + sy(shapes[kind][rot][i]));
		if(cy >= 0 && cx >= 0 && cx < COLS) board[cy][cx] = (uint8_t)(kind + 1);
	}
	for(y = 0; y < ROWS; y++)
	{
		uint8_t full = 1;
		for(x = 0; x < COLS; x++) if(!board[y][x]) { full = 0; break; }
		if(!full) continue;
		lines++;
		for(i = y; i > 0; i--)
			for(x = 0; x < COLS; x++) board[i][x] = board[i - 1][x];
		for(x = 0; x < COLS; x++) board[0][x] = 0;
	}
	paint_board();
	spawn();
}

static void drop(void)
{
	if(over) return;
	paint_piece(UI_BG);
	if(fits(px, (int8_t)(py + 1), rot))
	{
		py++;
		paint_piece(lv_color_hex(colors[kind]));
		}
	else
	{
		paint_piece(lv_color_hex(colors[kind]));
		lock_piece();
	}
}

void ui_tetris_step(void)
{
	drop();
}

static void new_game(void)
{
	uint8_t x, y;
	for(y = 0; y < ROWS; y++)
		for(x = 0; x < COLS; x++) board[y][x] = 0;
	lines = 0;
	over = 0;
	paint_board();
	if(lbl_over) lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
	spawn();
}

static lv_obj_t *build(void);

lv_obj_t *ui_tetris_create(void)
{
	return NULL;
}

lv_obj_t *ui_tetris_ensure(void)
{
	static lv_obj_t *scr;
	if(!scr) scr = build();
	else if(!board_view)
	{
		board_view = lv_canvas_create(scr);
		set_board(board_view);
		new_game();
	}
	return scr;
}



static lv_obj_t *build(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	ui_style_screen(scr);
	ui_status_create(scr);

	board_view = lv_canvas_create(scr);
	set_board(board_view);

	lbl_over = lv_label_create(scr);
	lv_obj_set_style_text_font(lbl_over, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_over, UI_OFFLINE, 0);
	lv_label_set_text(lbl_over, "\xE7\xBB\x93\xE6\x9D\x9F  \xE5\x86\x8D\xE6\x9D\xA5");
	lv_obj_align(lbl_over, LV_ALIGN_BOTTOM_MID, 0, -40);
	lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);

	ui_footer_create(scr, LV_SYMBOL_UP " \xE8\xBD\xAC  " LV_SYMBOL_DOWN " \xE8\x90\xBD  "
		LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " \xE7\xA7\xBB  \xE9\x95\xBF\xE6\x8C\x89" LV_SYMBOL_LEFT " \xE9\x80\x80\xE5\x87\xBA");
	new_game();
	return scr;
}

void ui_tetris_on_key(uint8_t key)
{
	uint8_t nr;
	if(over)
	{
		if(key == KEY_OK) new_game();
		return;
	}
	if(key == KEY_BACK || key == KEY_OK)
	{
		int8_t nx = (int8_t)(px + (key == KEY_OK ? 1 : -1));
		paint_piece(UI_BG);
		if(fits(nx, py, rot)) px = nx;
		paint_piece(lv_color_hex(colors[kind]));
		}
	else if(key == KEY_UP)
	{
		nr = (uint8_t)((rot + 1u) % 4u);
		paint_piece(UI_BG);
		if(fits(px, py, nr)) rot = nr;
		paint_piece(lv_color_hex(colors[kind]));
		}
	else if(key == KEY_DOWN)
	{
		drop();
	}
}

void ui_tetris_stop(void)
{
	if(board_view) { lv_obj_del(board_view); board_view = NULL; }
	lbl_over = NULL;
	over = 0;
}


