#ifndef _eeprom_H
#define _eeprom_H

#include "system.h"

/* 24C02，256 字节，与 MPU6050 共用软件 I2C（PB8/PB9），7 位地址 0x50。
   只存计步相关的一小块，布局固定，魔数不对就当没写过。 */

#define EEPROM_ADDR		0x50

#define EE_OFF_MAGIC	0x00	/* 4 字节 "STEP" */
#define EE_OFF_TOTAL	0x04	/* 总步数 u32 小端 */
#define EE_OFF_TODAY	0x08	/* 今日步数 u32 小端 */
#define EE_OFF_DATE		0x0C	/* 今日对应的日期 1..31 */
#define EE_OFF_TH		0x10	/* 动态阈值，0.001 g，u16 小端 */
#define EE_OFF_GAP		0x12	/* 最小步间隔 ms，u16 小端 */
#define EE_OFF_ALARM	0x14	/* 闹钟：1 字节开关 + 时 + 分 */
#define EE_LEN			0x18	/* 一次读写的长度 */

#define EE_TH_DEFAULT	150u
#define EE_GAP_DEFAULT	300u

/* 读出的一份镜像。valid=0 表示器件无应答或魔数不对，字段是默认值。 */
typedef struct
{
	u8  valid;
	u32 total;
	u32 today;
	u8  date;
	u16 th_mg;
	u16 gap_ms;
	u8  alarm_on;
	u8  alarm_h;
	u8  alarm_m;
} eeprom_data_t;

void EEPROM_Init(void);
void EEPROM_Load(eeprom_data_t *d);		/* 调度器启动前调用 */
void EEPROM_Save(const eeprom_data_t *d);	/* 只在 task_slow 里调用 */

#endif
