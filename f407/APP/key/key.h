#ifndef KEY_H
#define KEY_H

#include "system.h"

#define KEY_NONE 0u
#define KEY_UP   1u
#define KEY_DOWN 2u
#define KEY_OK   3u	/* 右 */
#define KEY_BACK 4u	/* 左 */

/* 按住满 600 ms 触发一次，松开不再补短按。 */
#define KEY_UP_LONG   5u
#define KEY_DOWN_LONG 6u
#define KEY_OK_LONG   7u
#define KEY_BACK_LONG 8u

#define KEY_LONG_MS 600u

void KEY_Init(void);
uint8_t KEY_ReadEdge(void);
uint8_t KEY_UpHeld(void);	/* PA0 当前是否按住，供录音判断松开 */
uint8_t KEY_Held(uint8_t key);	/* 短按码对应的键当前是否按住 */

#endif
