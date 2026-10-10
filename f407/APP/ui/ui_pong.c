#include "ui_games.h"
#include "ui_internal.h"
#include "key.h"
#include "system.h"

/* 单人挡板。球每 40 ms 走一步，按住左右键挡板连续移动。 */

#define FIELD_W 240
#define FIELD_H 360
#define BALL    10
#define PAD_W   56
#define PAD_H   8

static lv_obj_t *field;
static lv_obj_t *ball;
static lv_obj_t *pad;
static lv_obj_t *lbl_score;
static lv_obj_t *lbl_over;

static int16_t bx, by, px;
static int8_t vx, vy;
static uint16_t score;
static uint8_t over;
static uint32_t move_stamp;

static void place(void)
{
	lv_obj_set_pos(ball, bx, by);
	lv_obj_set_pos(pad, px, FIELD_H - 24);
	if(lbl_score) lv_label_set_text_fmt(lbl_score, "%u", score);
}

static void new_game(void)
{
	bx = FIELD_W / 2 - BALL / 2;
	by = 40;
	vx = 3; vy = 4;
	px = (FIELD_W - PAD_W) / 2;
	score = 0;
	over = 0;
	place();
	if(lbl_over) lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
}

void ui_pong_step(void)
{
	if(over) return;

	if(HAL_GetTick() - move_stamp >= 30u)
	{
		move_stamp = HAL_GetTick();
		if(KEY_Held(KEY_BACK) && px > 0) px = (int16_t)(px - 6);
		if(KEY_Held(KEY_OK) && px < FIELD_W - PAD_W) px = (int16_t)(px + 6);
	}

	bx = (int16_t)(bx + vx);
	by = (int16_t)(by + vy);
	if(bx <= 0) { bx = 0; vx = (int8_t)-vx; }
	if(bx >= FIELD_W - BALL) { bx = FIELD_W - BALL; vx = (int8_t)-vx; }
	if(by <= 0) { by = 0; vy = (int8_t)-vy; }

	if(by + BALL >= FIELD_H - 24 && by + BALL <= FIELD_H - 24 + PAD_H + 4 &&
	   bx + BALL >= px && bx <= px + PAD_W)
	{
		by = (int16_t)(FIELD_H - 24 - BALL);
		vy = (int8_t)-vy;
		if(vx > 0 && vx < 7) vx++;
		if(vx < 0 && vx > -7) vx--;
		score++;
	}
	else if(by > FIELD_H)
	{
		over = 1;
		if(lbl_over) lv_obj_clear_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);
	}
	place();
}

static lv_obj_t *build(void);

lv_obj_t *ui_pong_create(void)
{
	return NULL;	/* 真正的界面在第一次进入时由 ui_pong_ensure 建立 */
}

lv_obj_t *ui_pong_ensure(void)
{
	static lv_obj_t *scr;
	if(!scr) scr = build();
	else if(!ball)
	{
		ball = lv_obj_create(field);
		lv_obj_remove_style_all(ball);
		lv_obj_set_size(ball, BALL, BALL);
		lv_obj_set_style_bg_color(ball, UI_FG, 0);
		lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
		lv_obj_set_style_radius(ball, BALL / 2, 0);
		pad = lv_obj_create(field);
		lv_obj_remove_style_all(pad);
		lv_obj_set_size(pad, PAD_W, PAD_H);
		lv_obj_set_style_bg_color(pad, UI_ACCENT, 0);
		lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
		lv_obj_set_style_radius(pad, 3, 0);
		new_game();
	}
	return scr;
}



static lv_obj_t *build(void)
{
	lv_obj_t *scr = lv_obj_create(NULL);

	ui_style_screen(scr);
	ui_status_create(scr);

	lbl_score = lv_label_create(scr);
	lv_obj_set_style_text_font(lbl_score, &lv_font_montserrat_16, 0);
	lv_obj_set_style_text_color(lbl_score, UI_ACCENT, 0);
	lv_obj_align(lbl_score, LV_ALIGN_TOP_MID, 0, 40);

	field = ui_card(scr, FIELD_W, FIELD_H);
	lv_obj_align(field, LV_ALIGN_BOTTOM_MID, 0, -36);

	ball = lv_obj_create(field);
	lv_obj_remove_style_all(ball);
	lv_obj_set_size(ball, BALL, BALL);
	lv_obj_set_style_bg_color(ball, UI_FG, 0);
	lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(ball, BALL / 2, 0);

	pad = lv_obj_create(field);
	lv_obj_remove_style_all(pad);
	lv_obj_set_size(pad, PAD_W, PAD_H);
	lv_obj_set_style_bg_color(pad, UI_ACCENT, 0);
	lv_obj_set_style_bg_opa(pad, LV_OPA_COVER, 0);
	lv_obj_set_style_radius(pad, 3, 0);

	lbl_over = lv_label_create(field);
	lv_obj_set_style_text_font(lbl_over, &lv_font_chinese_16, 0);
	lv_obj_set_style_text_color(lbl_over, UI_OFFLINE, 0);
	lv_label_set_text(lbl_over, "\xE7\xBB\x93\xE6\x9D\x9F  \xE5\x86\x8D\xE6\x9D\xA5");
	lv_obj_align(lbl_over, LV_ALIGN_CENTER, 0, 0);
	lv_obj_add_flag(lbl_over, LV_OBJ_FLAG_HIDDEN);

	ui_footer_create(scr, LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " \xE6\x8C\xA1\xE6\x9D\xBF  " LV_SYMBOL_LEFT " \xE9\x80\x80\xE5\x87\xBA");
	new_game();
	move_stamp = HAL_GetTick();
	return scr;
}

void ui_pong_on_key(uint8_t key)
{
	if(over && key == KEY_OK) new_game();
}

void ui_pong_stop(void)
{
	if(ball) { lv_obj_del(ball); ball = NULL; }
	if(pad) { lv_obj_del(pad); pad = NULL; }
	over = 0;
	score = 0;
}


