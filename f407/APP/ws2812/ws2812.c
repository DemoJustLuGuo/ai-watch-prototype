#include "ws2812.h"
#include "SysTick.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* 时序沿用例程：高电平约 0.3 us 为 0，约 0.8 us 为 1。DWT 的 delay_us
   以微秒计，这里直接数周期，168 MHz 下一个周期约 6 ns。 */
/* 数值是实测过的循环开销。-O0 下一次比较约 7 个周期，
   所以这里粗略地按周期除以 7 来对齐 0.4 / 0.8 us。 */
#define T0H_CYC 10u
#define T1H_CYC 20u
#define TBIT_CYC 30u

#define PE5_BSRR (*(volatile uint32_t *)0x40021018UL)
#define PIN_HIGH() (PE5_BSRR = (1UL << 5))
#define PIN_LOW()  (PE5_BSRR = (1UL << (5 + 16)))

static uint8_t frame[RGB_LED_YHIGH][RGB_LED_XWIDTH][3];

/* 5 列宽的字形，每字节的高 5 位是从上到下的一行。 */
static const uint8_t glyph_c[5] = {0x70,0x88,0x88,0x88,0x00};
static const uint8_t glyph_q[5] = {0x70,0x88,0x88,0x78,0x08};
static const uint8_t glyph_u[5] = {0x88,0x88,0x88,0x88,0x70};
static const uint8_t glyph_t[5] = {0x80,0xF8,0x88,0x88,0x00};

static const uint8_t *const word[4] = {glyph_c, glyph_q, glyph_u, glyph_t};

static void wait_cyc(uint32_t cyc)
{
	uint32_t start = DWT->CYCCNT;
	while((DWT->CYCCNT - start) < cyc);
}

static void write_bit(uint8_t one)
{
	uint32_t high = one ? T1H_CYC : T0H_CYC;
	PIN_HIGH();
	wait_cyc(high);
	PIN_LOW();
	wait_cyc(TBIT_CYC - high);
}

static void write_byte(uint8_t byte)
{
	uint8_t i;
	for(i = 0; i < 8; i++)
	{
		write_bit(byte & 0x80);
		byte <<= 1;
	}
}

/* 关中断整帧送出。25 颗灯约 0.8 ms，不会拖垮界面刷新。 */
static void flush(void)
{
	uint8_t y, x;
	taskENTER_CRITICAL();
	for(y = 0; y < RGB_LED_YHIGH; y++)
		for(x = 0; x < RGB_LED_XWIDTH; x++)
		{
			write_byte(frame[y][x][1]);	/* G */
			write_byte(frame[y][x][0]);	/* R */
			write_byte(frame[y][x][2]);	/* B */
		}
	taskEXIT_CRITICAL();
	PIN_LOW();
	delay_us(80);	/* 复位码，锁存这一帧 */
}

void RGB_LED_Init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOE_CLK_ENABLE();
	gpio.Pin = GPIO_PIN_5;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOE, &gpio);
	PIN_LOW();

	memset(frame, 0, sizeof(frame));
	flush();
}

/* 滚动文本是 4 个字母加 1 列间隔，共 24 列。屏上第 x 列取文本的
   (shift + x) 列，shift 每次加一，到头回到 0。 */
static uint8_t column_on(uint16_t col, uint8_t row)
{
	uint8_t letter = (uint8_t)((col / 6u) % 4u);
	uint8_t sub = (uint8_t)(col % 6u);
	if(sub == 5) return 0;	/* 字母之间空一列 */
	return (word[letter][sub] >> (7 - row)) & 1u;
}

void RGB_Scroll_Task(void *arg)
{
	uint16_t shift = 0;
	(void)arg;
	for(;;)
	{
		uint8_t x, y;
		for(y = 0; y < RGB_LED_YHIGH; y++)
			for(x = 0; x < RGB_LED_XWIDTH; x++)
			{
				uint8_t on = column_on((uint16_t)(shift + x), y);
				frame[y][x][0] = on ? 0x40 : 0;	/* 青色，亮度压低以免刺眼 */
				frame[y][x][1] = on ? 0x40 : 0;
				frame[y][x][2] = 0;
			}
		flush();
		shift = (uint16_t)((shift + 1u) % 24u);
		vTaskDelay(pdMS_TO_TICKS(300));
	}
}
