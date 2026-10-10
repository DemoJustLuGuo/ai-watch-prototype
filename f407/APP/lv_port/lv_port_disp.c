#include "lvgl.h"
#include "tftlcd.h"

#define DISP_HOR 320
#define DISP_VER 480
#define DISP_BUF_LINES 20

/* 20 行双缓冲放内部 SRAM。全屏缓冲放外部 SRAM 时，LVGL 的行距与区域宽度
   不一致，按区域宽度去读会串行，画面错位。行距问题确认之前先用这个能正常
   显示的配置。 */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf_a[DISP_HOR * DISP_BUF_LINES];
static lv_color_t buf_b[DISP_HOR * DISP_BUF_LINES];

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
	u32 w = (u32)(area->x2 - area->x1 + 1);
	u32 h = (u32)(area->y2 - area->y1 + 1);
	u32 i;

	/* 屏幕第 0 行在物理底部，窗口用翻转后的坐标，像素按行倒序写进去。 */
	LCD_Set_Window((u16)area->x1,
	               (u16)((DISP_VER - 1) - area->y2),
	               (u16)area->x2,
	               (u16)((DISP_VER - 1) - area->y1));
	for(i = 0; i < h; i++)
	{
		lv_color_t *row = color_p + (h - 1u - i) * w;
		u32 x;
		for(x = 0; x < w; x++) LCD_WriteData_Color(row[x].full);
	}
	lv_disp_flush_ready(drv);
}

void lv_port_disp_init(void)
{
	static lv_disp_drv_t disp_drv;

	lv_disp_draw_buf_init(&draw_buf, buf_a, buf_b, DISP_HOR * DISP_BUF_LINES);
	lv_disp_drv_init(&disp_drv);
	disp_drv.hor_res = DISP_HOR;
	disp_drv.ver_res = DISP_VER;
	disp_drv.flush_cb = disp_flush;
	disp_drv.draw_buf = &draw_buf;
	lv_disp_drv_register(&disp_drv);
}
