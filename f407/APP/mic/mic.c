#include "mic.h"

#include <stdio.h>

/* TIM3 TRGO: 84 MHz / 5250 = 16 kHz. One circular DMA spans two
   1024-sample halves. DMA keeps filling one half while the ISR copies
   the other half to PCM. Only pcm belongs to the external UNINIT region. */
#define MIC_MAX_SAMPLES (MIC_MAX_BYTES / 2u)
#define MIC_BLK 1024u
#define MIC_RING (2u * MIC_BLK)
#define IRAM __attribute__((section(".bss.iram")))

static ADC_HandleTypeDef hadc IRAM;
static TIM_HandleTypeDef htim IRAM;
static DMA_HandleTypeDef hdma IRAM;
static u16 raw[MIC_RING] IRAM;
static volatile u16 pcm[MIC_MAX_SAMPLES];
static volatile u32 filled IRAM;
static volatile u32 error IRAM;
static volatile u8 running IRAM;
static u8 active IRAM;
static u8 initialized IRAM;
static u16 adc_min IRAM, adc_max IRAM;
static u32 adc_sum IRAM, adc_clip IRAM;
static u32 fault_sample IRAM;
static u16 fault_expected IRAM, fault_read IRAM;

/* Bounded register operations only; HAL cleanup runs later in Mic_Stop. */
static void halt_capture(void)
{
    __HAL_TIM_DISABLE(&htim);
    __HAL_ADC_DISABLE(&hadc);
    __HAL_DMA_DISABLE(&hdma);
    running = 0;
}

static void convert(u32 first, u32 n)
{
    u32 i, base = filled;
    if(n > MIC_MAX_SAMPLES - base) n = MIC_MAX_SAMPLES - base;
    for(i = 0; i < n; i++)
    {
        /* Multiplication is defined for negative values; signed << is not. */
        u16 adc = raw[(first + i) % MIC_RING];
        u16 value = (u16)(int16_t)(((int32_t)adc - 2048) * 16);
        u16 readback;
        pcm[base + i] = value;
        readback = pcm[base + i];
        if(readback != value)
        {
            fault_sample = base + i;
            fault_expected = value;
            fault_read = readback;
            error |= MIC_ERR_MEMORY;
            halt_capture();
            break;
        }
        if(adc < adc_min) adc_min = adc;
        if(adc > adc_max) adc_max = adc;
        adc_sum += adc;
        if(adc <= 8 || adc >= 4087) adc_clip++;
    }
    filled = base + i;
}

static void on_half(u32 first)
{
    u32 pos;
    if(!running) return;
    pos = (MIC_RING - hdma.Instance->NDTR) % MIC_RING;
    /* A delayed callback must never copy a half that DMA is overwriting. */
    if((filled % MIC_RING) != first ||
       (first == 0 ? pos < MIC_BLK : pos >= MIC_BLK))
    {
        error |= MIC_ERR_LATE;
        halt_capture();
        return;
    }
    convert(first, MIC_BLK);
    pos = (MIC_RING - hdma.Instance->NDTR) % MIC_RING;
    if(first == 0 ? pos < MIC_BLK : pos >= MIC_BLK)
    {
        error |= MIC_ERR_LATE;
        halt_capture();
    }
    if(filled >= MIC_MAX_SAMPLES) halt_capture();
}

void Mic_Init(void)
{
	GPIO_InitTypeDef gpio = {0};
	ADC_ChannelConfTypeDef ch = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_ADC1_CLK_ENABLE();
	__HAL_RCC_TIM3_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();

	initialized = 0;
	active = running = 0;
	filled = error = 0;

	gpio.Pin = GPIO_PIN_5;
	gpio.Mode = GPIO_MODE_ANALOG;
	gpio.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &gpio);

	hdma.Instance = DMA2_Stream0;
	hdma.Init.Channel = DMA_CHANNEL_0;		/* Stream0 + Channel0 = ADC1 */
	hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hdma.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma.Init.MemInc = DMA_MINC_ENABLE;
	hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
	hdma.Init.Mode = DMA_CIRCULAR;
	hdma.Init.Priority = DMA_PRIORITY_HIGH;
	hdma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	hdma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
	hdma.Init.MemBurst = DMA_MBURST_SINGLE;
	hdma.Init.PeriphBurst = DMA_PBURST_SINGLE;
	if(HAL_DMA_Init(&hdma) != HAL_OK) error |= MIC_ERR_INIT;

	hadc.Instance = ADC1;
	hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc.Init.Resolution = ADC_RESOLUTION_12B;
	hadc.Init.ScanConvMode = DISABLE;
	hadc.Init.ContinuousConvMode = DISABLE;		/* 每次由 TIM3 TRGO 触发一次 */
	hadc.Init.DiscontinuousConvMode = DISABLE;
	hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
	hadc.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
	hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc.Init.NbrOfConversion = 1;
	hadc.Init.DMAContinuousRequests = ENABLE;
	hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	if(HAL_ADC_Init(&hadc) != HAL_OK) error |= MIC_ERR_INIT;
	__HAL_LINKDMA(&hadc, DMA_Handle, hdma);

	ch.Channel = ADC_CHANNEL_5;
	ch.Rank = 1;
	ch.SamplingTime = ADC_SAMPLETIME_84CYCLES;	/* 远短于 62.5 us 的采样周期 */
	if(HAL_ADC_ConfigChannel(&hadc, &ch) != HAL_OK) error |= MIC_ERR_INIT;

	/* 84 MHz / 16000 = 5250 */
	htim.Instance = TIM3;
	htim.Init.Prescaler = 0;
	htim.Init.Period = 5250U - 1U;
	htim.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	if(HAL_TIM_Base_Init(&htim) != HAL_OK) error |= MIC_ERR_INIT;

	{
		TIM_MasterConfigTypeDef m = {0};
		m.MasterOutputTrigger = TIM_TRGO_UPDATE;
		m.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		if(HAL_TIMEx_MasterConfigSynchronization(&htim, &m) != HAL_OK) error |= MIC_ERR_INIT;
	}

    /* UART4 (priority 6) can preempt the PCM copy. */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority(ADC_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
    initialized = (error == 0);
}

u8 Mic_Start(void)
{
    if(active) Mic_Stop();
    filled = 0;
    if(!initialized) { error = MIC_ERR_INIT; return 0; }
    error = 0;
    adc_min = 4095; adc_max = 0;
    adc_sum = adc_clip = 0;
    fault_sample = 0; fault_expected = fault_read = 0;
    __HAL_TIM_SET_COUNTER(&htim, 0);
    __HAL_TIM_CLEAR_FLAG(&htim, TIM_FLAG_UPDATE);
    active = running = 1;
    if(HAL_ADC_Start_DMA(&hadc, (uint32_t *)raw, MIC_RING) != HAL_OK ||
       hdma.State != HAL_DMA_STATE_BUSY || HAL_TIM_Base_Start(&htim) != HAL_OK)
    {
        error |= MIC_ERR_START;
        halt_capture();
        Mic_Stop();
        return 0;
    }
    return 1;
}

void Mic_Stop(void)
{
    u32 pos, first, n, flags;
    if(!active) return;
    /* Freeze DMA and snapshot NDTR after HAL has disabled the stream.
       Pending half callbacks are accounted for by the ring cursor below. */
    HAL_NVIC_DisableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_DisableIRQ(ADC_IRQn);
    HAL_TIM_Base_Stop(&htim);
    __HAL_ADC_DISABLE(&hadc);
    if(__HAL_ADC_GET_FLAG(&hadc, ADC_FLAG_OVR)) error |= MIC_ERR_ADC;
    flags = DMA2->LISR;
    if(flags & (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0))
        error |= MIC_ERR_DMA;
    if((flags & (DMA_LISR_HTIF0 | DMA_LISR_TCIF0)) ==
                (DMA_LISR_HTIF0 | DMA_LISR_TCIF0)) error |= MIC_ERR_LATE;
    if(HAL_ADC_Stop_DMA(&hadc) != HAL_OK) error |= MIC_ERR_DMA;
    pos = (MIC_RING - hdma.Instance->NDTR) % MIC_RING;
    first = filled % MIC_RING;
    n = (pos + MIC_RING - first) % MIC_RING;
    if(!error && filled < MIC_MAX_SAMPLES) convert(first, n);
    running = active = 0;
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_ClearPendingIRQ(ADC_IRQn);
    HAL_NVIC_EnableIRQ(ADC_IRQn);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

u8 Mic_Busy(void) { return running; }
u32 Mic_Bytes(void) { return filled * 2u; }
const u8 *Mic_Data(void) { return (const u8 *)pcm; }
u32 Mic_Error(void) { return error; }

void Mic_Report(void)
{
    u32 n = filled;
    if(active) return;
    printf("[mic] samples=%lu bytes=%lu ms=%lu adc_min=%u max=%u mean=%lu clip=%lu err=0x%02lX\r\n",
           (unsigned long)n, (unsigned long)(n * 2u),
           (unsigned long)(n * 1000u / MIC_RATE), n ? adc_min : 0, adc_max,
           (unsigned long)(n ? adc_sum / n : 0), (unsigned long)adc_clip,
           (unsigned long)error);
    if(error & MIC_ERR_MEMORY)
        printf("[mic] memory sample=%lu expected=0x%04X read=0x%04X\r\n",
               (unsigned long)fault_sample, fault_expected, fault_read);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
{
    if(h == &hadc) on_half(0);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if(h == &hadc) on_half(MIC_BLK);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h)
{
    if(h != &hadc || !active) return;
    error |= (h->ErrorCode & HAL_ADC_ERROR_DMA) ? MIC_ERR_DMA : MIC_ERR_ADC;
    halt_capture();
}

void DMA2_Stream0_IRQHandler(void)
{
    if((DMA2->LISR & (DMA_LISR_HTIF0 | DMA_LISR_TCIF0)) ==
                    (DMA_LISR_HTIF0 | DMA_LISR_TCIF0) && running)
    {
        error |= MIC_ERR_LATE;
        halt_capture();
    }
    HAL_DMA_IRQHandler(&hdma);
}

void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc);
}
