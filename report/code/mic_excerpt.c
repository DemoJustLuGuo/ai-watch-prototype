/* 节选自 f407/APP/mic/mic.c —— DMA 半块转存：12bit ADC→16bit PCM，写后读校验 */

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

/* 半满/全满回调：先核对 DMA 游标没有追上要拷的半块，再转存。
   延迟到达的回调绝不能拷 DMA 正在覆写的半块。 */
static void on_half(u32 first)
{
    u32 pos;
    if(!running) return;
    pos = (MIC_RING - hdma.Instance->NDTR) % MIC_RING;
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
    }
    if(filled >= MIC_MAX_SAMPLES) halt_capture();
}
