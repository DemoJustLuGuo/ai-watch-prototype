#ifndef _ds18b20_H
#define _ds18b20_H

#include "system.h"

////IO方向设置
#define	DS18B20_DQ_OUT PGout(9) //数据端口
#define	DS18B20_DQ_IN  PGin(9)  //数据端口

u8 DS18B20_Init(void);			//初始化DS18B20
void DS18B20_Start(void);		//开始一次温度转换（约1ms，不等待转换完成）
u8 DS18B20_Read(int16_t *deci);	//读上次转换结果，单位 0.1°C
u8 DS18B20_Present(void);		//总线上是否检测到器件

#endif
