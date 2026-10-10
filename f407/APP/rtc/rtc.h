#ifndef _rtc_H
#define _rtc_H

#include "system.h"

typedef struct
{
	u8 hour;
	u8 min;
	u8 sec;
	u8 day;		/* 1..31，跨日归零今日步数用 */
} rtc_time_t;

u8 RTC_Init(void);					//初始化，首次上电用编译时间设初值
void RTC_Get(rtc_time_t *t);		//读当前时分秒
void RTC_Set(u8 hour,u8 min,u8 sec);//手动对时

#endif
