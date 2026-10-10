#include "rtc.h"

/* LSE 32.768kHz 已焊，VBAT 有电池座，后备域在主电断开后维持日历。
   方案 4.6：首次上电（备份寄存器魔数不对）用编译时间 __TIME__ 设初值，
   避免答辩时时间是 2000 年；之后轮询时分秒，不开 Alarm 中断。 */

#define RTC_BKP_MAGIC	0x5051		//与普中例程的 0x5050 区分，表示本工程已设过时

static RTC_HandleTypeDef RTC_Handler;

//RTC底层驱动，时钟配置，此函数会被 HAL_RTC_Init() 调用
void HAL_RTC_MspInit(RTC_HandleTypeDef* hrtc)
{
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	(void)hrtc;
    __HAL_RCC_PWR_CLK_ENABLE();//使能电源时钟PWR
	HAL_PWR_EnableBkUpAccess();//取消备份区域写保护

    RCC_OscInitStruct.OscillatorType=RCC_OSCILLATORTYPE_LSE;//LSE配置
    RCC_OscInitStruct.PLL.PLLState=RCC_PLL_NONE;
    RCC_OscInitStruct.LSEState=RCC_LSE_ON;                  //RTC使用LSE
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    PeriphClkInitStruct.PeriphClockSelection=RCC_PERIPHCLK_RTC;//外设为RTC
    PeriphClkInitStruct.RTCClockSelection=RCC_RTCCLKSOURCE_LSE;//RTC时钟源为LSE
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    __HAL_RCC_RTC_ENABLE();//RTC时钟使能
}

void RTC_Set(u8 hour,u8 min,u8 sec)
{
	RTC_TimeTypeDef RTC_TimeStructure;

	RTC_TimeStructure.Hours=hour;
	RTC_TimeStructure.Minutes=min;
	RTC_TimeStructure.Seconds=sec;
	RTC_TimeStructure.TimeFormat=RTC_HOURFORMAT12_AM;
	RTC_TimeStructure.DayLightSaving=RTC_DAYLIGHTSAVING_NONE;
    RTC_TimeStructure.StoreOperation=RTC_STOREOPERATION_RESET;
	HAL_RTC_SetTime(&RTC_Handler,&RTC_TimeStructure,RTC_FORMAT_BIN);
}

//把编译时间 __TIME__（"HH:MM:SS"）解析成时分秒
static void RTC_Set_From_Build(void)
{
	const char *t=__TIME__;	//编译期字符串常量
	u8 hour=(u8)((t[0]-'0')*10+(t[1]-'0'));
	u8 min =(u8)((t[3]-'0')*10+(t[4]-'0'));
	u8 sec =(u8)((t[6]-'0')*10+(t[7]-'0'));
	RTC_Set(hour,min,sec);
}

//RTC初始化
//返回值:0,初始化成功;1,进入初始化模式失败
u8 RTC_Init(void)
{
	RTC_Handler.Instance=RTC;
    RTC_Handler.Init.HourFormat=RTC_HOURFORMAT_24;//24小时制
    RTC_Handler.Init.AsynchPrediv=0X7F;           //异步分频系数(1~0X7F)
    RTC_Handler.Init.SynchPrediv=0XFF;            //同步分频系数(0~7FFF)
    RTC_Handler.Init.OutPut=RTC_OUTPUT_DISABLE;
    RTC_Handler.Init.OutPutPolarity=RTC_OUTPUT_POLARITY_HIGH;
    RTC_Handler.Init.OutPutType=RTC_OUTPUT_TYPE_OPENDRAIN;
    if(HAL_RTC_Init(&RTC_Handler)!=HAL_OK)
		return 1;

    if(HAL_RTCEx_BKUPRead(&RTC_Handler,RTC_BKP_DR0)!=RTC_BKP_MAGIC)//是否第一次配置
    {
        RTC_Set_From_Build();	//用编译时间设初值
        HAL_RTCEx_BKUPWrite(&RTC_Handler,RTC_BKP_DR0,RTC_BKP_MAGIC);//标记已经初始化过了
    }
    return 0;
}

void RTC_Get(rtc_time_t *t)
{
	RTC_TimeTypeDef RTC_TimeStructure;
	RTC_DateTypeDef RTC_DateStructure;

	HAL_RTC_GetTime(&RTC_Handler,&RTC_TimeStructure,RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&RTC_Handler,&RTC_DateStructure,RTC_FORMAT_BIN);//读时间后必须读日期，否则日历被锁
	t->hour=RTC_TimeStructure.Hours;
	t->min =RTC_TimeStructure.Minutes;
	t->sec =RTC_TimeStructure.Seconds;
	t->day =RTC_DateStructure.Date;
}
