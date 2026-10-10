#include "beep.h"

void BEEP_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	BEEP_PORT_RCC_ENABLE;

	GPIO_InitStructure.Pin=BEEP_PIN;
	GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;	//推挽输出
	GPIO_InitStructure.Pull=GPIO_PULLUP;
	GPIO_InitStructure.Speed=GPIO_SPEED_HIGH;
	HAL_GPIO_Init(BEEP_PORT,&GPIO_InitStructure);

	BEEP_Off();		//复位后默认不响
}

void BEEP_On(void)
{
	HAL_GPIO_WritePin(BEEP_PORT,BEEP_PIN,GPIO_PIN_SET);		//实测本板高电平鸣叫
}

void BEEP_Off(void)
{
	HAL_GPIO_WritePin(BEEP_PORT,BEEP_PIN,GPIO_PIN_RESET);	//低电平静音
}

uint8_t BEEP_IsOn(void)
{
	return HAL_GPIO_ReadPin(BEEP_PORT, BEEP_PIN) == GPIO_PIN_SET;
}
