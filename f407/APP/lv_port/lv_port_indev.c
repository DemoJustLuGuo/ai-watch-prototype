#include "lvgl.h"
#include "key.h"
#include "ui.h"

/* Keys are handled in ui_on_key(). The indev stays released so LVGL's
 * group does not also move the focus. */

static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
	uint8_t key;

	(void)drv;
	data->state = LV_INDEV_STATE_REL;
	data->key = 0;
	key = KEY_ReadEdge();
	if(key != KEY_NONE) ui_on_key(key);
}

void lv_port_indev_init(void)
{
	static lv_indev_drv_t indev_drv;

	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_KEYPAD;
	indev_drv.read_cb = keypad_read;
	lv_indev_drv_register(&indev_drv);
}
