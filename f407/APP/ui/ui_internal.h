#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "lvgl.h"

extern const lv_font_t lv_font_chinese_16;

#define UI_BG      lv_color_hex(0x0B1220)
#define UI_FG      lv_color_hex(0xE8EEF7)
#define UI_ACCENT  lv_color_hex(0x3DDC97)
#define UI_OFFLINE lv_color_hex(0xE6B450)
#define UI_CARD    lv_color_hex(0x162033)	/* 卡片底色，比屏幕底亮一档 */
#define UI_MUTED   lv_color_hex(0x8FA0B8)	/* 次级文字 */
#define UI_ACCENT_DIM lv_color_hex(0x1C6B4E)	/* 强调色的暗色，用于未选中的描边 */
#define UI_LINE    lv_color_hex(0x243049)	/* 分隔线和进度条底槽 */

void ui_style_screen(lv_obj_t *scr);
lv_obj_t *ui_status_create(lv_obj_t *parent);
void ui_footer_create(lv_obj_t *parent, const char *text);

/* 圆角卡片，去边框、去滚动。只在界面任务里调用。 */
lv_obj_t *ui_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

#endif
