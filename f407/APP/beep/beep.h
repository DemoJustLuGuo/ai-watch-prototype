#ifndef _beep_H
#define _beep_H

#include "system.h"

/* 蜂鸣器 BZ1 接 PF8（方案 3.2）。原理图推断是 PNP 高边低电平响，
   但实测本板上电即响：复位后 10kΩ 把 PF8 上拉到高就在鸣叫，
   所以本板是"高电平响、低电平静"。封装成"响/停"，上层不关心极性。 */

#define BEEP_PORT 				GPIOF
#define BEEP_PIN 				GPIO_PIN_8
#define BEEP_PORT_RCC_ENABLE	__HAL_RCC_GPIOF_CLK_ENABLE()

void BEEP_Init(void);
void BEEP_On(void);		//鸣叫
void BEEP_Off(void);	//停
uint8_t BEEP_IsOn(void);	//当前是否在响

#endif
