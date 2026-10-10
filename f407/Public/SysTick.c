#include "SysTick.h"

static u32 fac_us=0;

/* 微秒延时改走 DWT 周期计数器。SysTick 归 FreeRTOS 后，计数器在调度器
   启动时被重装成 1 ms，再拿它做 delay_us 会把位时序拉长上千倍。 */
void SysTick_Init(u8 SYSCLK)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	fac_us = SYSCLK;
}

void delay_us(u32 nus)
{
	u32 start = DWT->CYCCNT;
	u32 ticks = nus * fac_us;
	while((DWT->CYCCNT - start) < ticks);
}

void delay_ms(u16 nms)
{
	while(nms--) delay_us(1000);
}
