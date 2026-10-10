#include "stm32f4xx_hal.h"

/* HAL 的 1 ms 时基改走 TIM6，把 SysTick 让给 FreeRTOS。
   覆盖 stm32f4xx_hal.c 里的 weak 实现，delay_us/HAL_Delay 在调度器
   启动前后都还能用。TIM6 挂在 APB1，分频不为 1 时定时器时钟是 84 MHz。 */

static TIM_HandleTypeDef TimHandle;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
	RCC_ClkInitTypeDef clkconfig;
	uint32_t timclk, latency;

	if(TimHandle.Instance != 0)
	{
		HAL_TIM_Base_Stop_IT(&TimHandle);
		HAL_TIM_Base_DeInit(&TimHandle);
	}

	__HAL_RCC_TIM6_CLK_ENABLE();

	HAL_RCC_GetClockConfig(&clkconfig, &latency);
	timclk = HAL_RCC_GetPCLK1Freq();
	if(clkconfig.APB1CLKDivider != RCC_HCLK_DIV1) timclk *= 2U;

	TimHandle.Instance = TIM6;
	TimHandle.Init.Prescaler = (timclk / 1000000U) - 1U;	/* 1 MHz */
	TimHandle.Init.Period = 1000U - 1U;						/* 1 ms */
	TimHandle.Init.ClockDivision = 0U;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if(HAL_TIM_Base_Init(&TimHandle) != HAL_OK) return HAL_ERROR;

	HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	return HAL_TIM_Base_Start_IT(&TimHandle);
}

void HAL_SuspendTick(void)
{
	__HAL_TIM_DISABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
	__HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM6) HAL_IncTick();
}

void TIM6_DAC_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&TimHandle);
}
