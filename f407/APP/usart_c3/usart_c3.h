#ifndef _usart_c3_H
#define _usart_c3_H

#include "system.h"

/* F407 与 ESP32-C3 走 UART4（PC10=TX/AF8，PC11=RX/AF8），460800 8N1。
   收到的整帧在中断里分派到模块状态，主循环用下面的访问器取。 */

void C3_Init(u32 bound);			//bound 用 460800
void C3_Tick(void);					//主循环调用；未连上时每 2s 发一次 HELLO

//状态
u8 C3_IsOnline(void);				//收到 HELLO_ACK 且在线标志=1
u8 C3_TakeWeather(char *out, u16 cap);	//有新天气串返回1并拷出，否则0
u8 C3_TakeAsr(char *out, u16 cap);		//有新识别文本返回1并拷出，否则0

//主动请求
void C3_RequestWeather(const char *city);	//发 CMD_WEATHER，city 为空则空载荷（C3 用默认城市）
void C3_SendFallAlert(u32 unix_or_hms);	//发 ALERT_FALL
u8 C3_SendAudio(u8 phase, const u8 *payload, u16 len, u8 *seq, u8 retry);
u8 C3_TakeAck(u8 seq, u8 *err); /* ACK must echo seq; current C3 NACK does not. */
void C3_AudioSession(u8 active); /* New session clears stale ACK/ASR queues. */
void C3_Report(void);           /* Task context only. */

#endif
