#include "key.h"

/* Vendor names: KEY_UP=PA0 high, KEY0=PE4, KEY1=PE3, KEY2=PE2. Pressed low except KEY_UP.
   方位：KEY_UP 上，KEY1 下，KEY0 右，KEY2 左。 */

void KEY_Init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

	gpio.Pin = GPIO_PIN_0;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_PULLDOWN;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio);

	gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
	gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOE, &gpio);
}

static uint8_t sample(void)
{
	uint8_t now = 0;

	if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) now |= 1u;
	if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) now |= 2u;
	if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET) now |= 4u;
	if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET) now |= 8u;
	return now;
}

static uint8_t code_of(uint8_t bit)
{
	if(bit & 1u) return KEY_UP;
	if(bit & 2u) return KEY_DOWN;
	if(bit & 4u) return KEY_OK;
	if(bit & 8u) return KEY_BACK;
	return KEY_NONE;
}

uint8_t KEY_ReadEdge(void)
{
	static uint8_t debounced = 0;
	static uint8_t last = 0;
	static uint32_t stamp = 0;
	static uint32_t hold_stamp = 0;
	static uint8_t hold_bit = 0;
	static uint8_t long_sent = 0;
	uint8_t now = sample();
	uint8_t edge;

	if(now != last) {
		last = now;
		stamp = HAL_GetTick();
		return KEY_NONE;
	}
	if((HAL_GetTick() - stamp) < 10u) return KEY_NONE;

	edge = (uint8_t)(now & (uint8_t)~debounced);
	if((hold_bit & debounced) == 0u) hold_bit = 0;
	debounced = now;

	if(edge) {
		hold_bit = edge;
		hold_stamp = HAL_GetTick();
		long_sent = 0;
		return KEY_NONE;	/* 等松开才算短按，按下沿不再发一次 */
	}

	if(hold_bit && (hold_bit & debounced)) {
		if(!long_sent && (HAL_GetTick() - hold_stamp) >= KEY_LONG_MS) {
			long_sent = 1;
			return (uint8_t)(code_of(hold_bit) + 4u);
		}
		return KEY_NONE;
	}

	if(hold_bit && !long_sent) {
		uint8_t key = code_of(hold_bit);
		hold_bit = 0;
		return key;
	}
	hold_bit = 0;
	return KEY_NONE;
}

uint8_t KEY_UpHeld(void)
{
	return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)==GPIO_PIN_SET;
}

uint8_t KEY_Held(uint8_t key)
{
	uint8_t now = sample();

	if(key == KEY_UP) return (now & 1u) ? 1u : 0u;
	if(key == KEY_DOWN) return (now & 2u) ? 1u : 0u;
	if(key == KEY_OK) return (now & 4u) ? 1u : 0u;
	if(key == KEY_BACK) return (now & 8u) ? 1u : 0u;
	return 0;
}
