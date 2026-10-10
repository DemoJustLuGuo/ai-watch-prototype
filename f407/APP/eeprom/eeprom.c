#include "eeprom.h"
#include "iic.h"
#include "SysTick.h"

/* 24C02 页写 8 字节，写完后器件拉低 SDA 直到内部写周期结束，
   用 ACK 轮询等它松开，不空等固定时间。 */

#define EE_MAGIC0	'S'
#define EE_MAGIC1	'T'
#define EE_MAGIC2	'E'
#define EE_MAGIC3	'P'

/* 从 addr 连续读 len 字节。器件无应答返回 1。 */
static u8 ee_read(u8 addr, u8 *buf, u8 len)
{
	IIC_Start();
	IIC_Send_Byte(EEPROM_ADDR << 1);
	if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
	IIC_Send_Byte(addr);
	if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }

	IIC_Start();
	IIC_Send_Byte((EEPROM_ADDR << 1) | 1);
	if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
	while(len)
	{
		*buf++ = IIC_Read_Byte(len > 1 ? 1 : 0);
		len--;
	}
	IIC_Stop();
	return 0;
}

/* 从 addr 连续写 len 字节，自动按 8 字节页切开。返回 1 表示无应答。 */
static u8 ee_write(u8 addr, const u8 *buf, u8 len)
{
	while(len)
	{
		u8 page = 8u - (addr & 7u);
		u8 n = len < page ? len : page;
		u16 spin;

		IIC_Start();
		IIC_Send_Byte(EEPROM_ADDR << 1);
		if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
		IIC_Send_Byte(addr);
		if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
		while(n--)
		{
			IIC_Send_Byte(*buf++);
			if(IIC_Wait_Ack()) { IIC_Stop(); return 1; }
			addr++;
			len--;
		}
		IIC_Stop();

		/* ACK 轮询：器件写完才应答。上限约 10 ms，超时按失败处理。 */
		for(spin = 0; spin < 1000; spin++)
		{
			u8 nak;
			delay_us(10);
			IIC_Start();
			IIC_Send_Byte(EEPROM_ADDR << 1);
			nak = IIC_Wait_Ack();
			IIC_Stop();
			if(!nak) break;
		}
	}
	return 0;
}

static void put_u32(u8 *p, u32 v)
{
	p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

static u32 get_u32(const u8 *p)
{
	return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

void EEPROM_Init(void)
{
	IIC_Init();
}

void EEPROM_Load(eeprom_data_t *d)
{
	u8 raw[EE_LEN];

	d->valid = 0;
	d->total = 0;
	d->today = 0;
	d->date = 0;
	d->th_mg = EE_TH_DEFAULT;
	d->gap_ms = EE_GAP_DEFAULT;
	d->alarm_on = 0;
	d->alarm_h = 7;
	d->alarm_m = 0;

	if(ee_read(EE_OFF_MAGIC, raw, EE_LEN)) return;
	if(raw[0] != EE_MAGIC0 || raw[1] != EE_MAGIC1 ||
	   raw[2] != EE_MAGIC2 || raw[3] != EE_MAGIC3) return;

	d->total = get_u32(raw + EE_OFF_TOTAL);
	d->today = get_u32(raw + EE_OFF_TODAY);
	d->date = raw[EE_OFF_DATE];
	d->th_mg = (u16)(raw[EE_OFF_TH] | (raw[EE_OFF_TH + 1] << 8));
	d->gap_ms = (u16)(raw[EE_OFF_GAP] | (raw[EE_OFF_GAP + 1] << 8));
	if(d->date < 1 || d->date > 31) d->date = 0;
	if(d->th_mg < 50 || d->th_mg > 500) d->th_mg = EE_TH_DEFAULT;
	if(d->gap_ms < 200 || d->gap_ms > 500) d->gap_ms = EE_GAP_DEFAULT;
	d->alarm_on = raw[EE_OFF_ALARM] ? 1 : 0;
	d->alarm_h = raw[EE_OFF_ALARM + 1];
	d->alarm_m = raw[EE_OFF_ALARM + 2];
	if(d->alarm_h > 23) d->alarm_h = 7;
	if(d->alarm_m > 59) d->alarm_m = 0;
	d->valid = 1;
}

void EEPROM_Save(const eeprom_data_t *d)
{
	u8 raw[EE_LEN];

	raw[0] = EE_MAGIC0; raw[1] = EE_MAGIC1; raw[2] = EE_MAGIC2; raw[3] = EE_MAGIC3;
	put_u32(raw + EE_OFF_TOTAL, d->total);
	put_u32(raw + EE_OFF_TODAY, d->today);
	raw[EE_OFF_DATE] = d->date;
	raw[EE_OFF_DATE + 1] = 0;
	raw[EE_OFF_DATE + 2] = 0;
	raw[EE_OFF_DATE + 3] = 0;
	raw[EE_OFF_TH] = (u8)d->th_mg;
	raw[EE_OFF_TH + 1] = (u8)(d->th_mg >> 8);
	raw[EE_OFF_GAP] = (u8)d->gap_ms;
	raw[EE_OFF_GAP + 1] = (u8)(d->gap_ms >> 8);
	raw[EE_OFF_ALARM] = d->alarm_on;
	raw[EE_OFF_ALARM + 1] = d->alarm_h;
	raw[EE_OFF_ALARM + 2] = d->alarm_m;
	raw[EE_OFF_ALARM + 3] = 0;
	ee_write(EE_OFF_MAGIC, raw, EE_LEN);
}
