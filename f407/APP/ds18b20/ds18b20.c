#include "ds18b20.h"
#include "SysTick.h"

/* PG9 单总线，板上 10kΩ 上拉（R54），只有一颗传感器。
   时序照普中例程 29。按方案 4.5 拆成两步：Start 只发转换命令，
   主循环过 750ms 之后再调 Read 取结果并启动下一轮，期间 GUI 照常跑。 */

static u8 ds18b20_present = 0;	//总线上是否检测到器件

static void DS18B20_IO_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin=GPIO_PIN_9;           //PG9
    GPIO_InitStructure.Mode=GPIO_MODE_INPUT;  	//输入
    GPIO_InitStructure.Pull=GPIO_PULLUP;          //上拉
    GPIO_InitStructure.Speed=GPIO_SPEED_HIGH;     //高速
    HAL_GPIO_Init(GPIOG,&GPIO_InitStructure);
}

static void DS18B20_IO_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin=GPIO_PIN_9;           //PG9
    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;  //推挽输出
    GPIO_InitStructure.Pull=GPIO_PULLUP;          //上拉
    GPIO_InitStructure.Speed=GPIO_SPEED_HIGH;     //高速
    HAL_GPIO_Init(GPIOG,&GPIO_InitStructure);
}

//复位DS18B20
static void DS18B20_Reset(void)
{
	DS18B20_IO_OUT(); //SET PG9 OUTPUT
	DS18B20_DQ_OUT=0; //拉低DQ
	delay_us(750);    //延时750us
	DS18B20_DQ_OUT=1; //DQ=1
	delay_us(15);     //15US
}

//检查DS18B20是否在总线上
//返回值:1:未检测到DS18B20的存在；0:存在
static u8 DS18B20_Check(void)
{
	u8 retry=0;
	DS18B20_IO_IN();
    while (DS18B20_DQ_IN&&retry<200)
	{
		retry++;
		delay_us(1);
	};
	if(retry>=200)return 1;
	else retry=0;
    while (!DS18B20_DQ_IN&&retry<240)
	{
		retry++;
		delay_us(1);
	};
	if(retry>=240)return 1;
	return 0;
}

//从DS18B20读取一个位
static u8 DS18B20_Read_Bit(void)
{
	u8 data;
	DS18B20_IO_OUT();
	DS18B20_DQ_OUT=0;
	delay_us(2);
	DS18B20_DQ_OUT=1;
	DS18B20_IO_IN();
	delay_us(12);
	if(DS18B20_DQ_IN)data=1;
	else data=0;
	delay_us(50);
	return data;
}

//从DS18B20读取一个字节
static u8 DS18B20_Read_Byte(void)
{
    u8 i,j,dat;
    dat=0;
	for (i=1;i<=8;i++)
	{
        j=DS18B20_Read_Bit();
        dat=(j<<7)|(dat>>1);
    }
    return dat;
}

//写一个字节到DS18B20
static void DS18B20_Write_Byte(u8 dat)
{
	u8 j;
    u8 testb;
	DS18B20_IO_OUT();
    for (j=1;j<=8;j++)
	{
        testb=dat&0x01;
        dat=dat>>1;
        if (testb)
        {
            DS18B20_DQ_OUT=0;// Write 1
            delay_us(2);
            DS18B20_DQ_OUT=1;
            delay_us(60);
        }
        else
        {
            DS18B20_DQ_OUT=0;// Write 0
            delay_us(60);
            DS18B20_DQ_OUT=1;
            delay_us(2);
        }
    }
}

/*******************************************************************************
* 函 数 名         : DS18B20_Init
* 功能说明		   : 初始化 DS18B20 的 IO 口，并启动第一次转换
* 返 回 值         : 1:器件不存在；0:存在
*******************************************************************************/
u8 DS18B20_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	__HAL_RCC_GPIOG_CLK_ENABLE();			//使能GPIOG时钟

	GPIO_InitStructure.Pin=GPIO_PIN_9;           //PG9
    GPIO_InitStructure.Mode=GPIO_MODE_OUTPUT_PP;  //推挽输出
    GPIO_InitStructure.Pull=GPIO_PULLUP;          //上拉
    GPIO_InitStructure.Speed=GPIO_SPEED_HIGH;     //高速
    HAL_GPIO_Init(GPIOG,&GPIO_InitStructure);

 	DS18B20_Reset();
	ds18b20_present=(DS18B20_Check()==0);
	if(ds18b20_present) DS18B20_Start();
	return ds18b20_present?0:1;
}

/*******************************************************************************
* 函 数 名         : DS18B20_Start
* 功能说明		   : 发起一次温度转换，立即返回。12 位分辨率最多 750ms 完成
*******************************************************************************/
void DS18B20_Start(void)
{
	DS18B20_Reset();
	DS18B20_Check();
    DS18B20_Write_Byte(0xcc);// skip rom
    DS18B20_Write_Byte(0x44);// convert
}

/*******************************************************************************
* 函 数 名         : DS18B20_Read
* 功能说明		   : 读取最近一次转换结果并立刻启动下一轮转换
* 形    参         : deci：输出，0.1°C 为单位的温度（-550~1250）
* 返 回 值         : 0:成功；1:总线上没有器件
*******************************************************************************/
u8 DS18B20_Read(int16_t *deci)
{
	u16 temp;
	u8 a,b;

	if(!ds18b20_present) return 1;

	DS18B20_Reset();
	if(DS18B20_Check())
	{
		ds18b20_present=0;
		return 1;
	}
    DS18B20_Write_Byte(0xcc);// skip rom
    DS18B20_Write_Byte(0xbe);// read scratchpad
    a=DS18B20_Read_Byte();   // LSB
    b=DS18B20_Read_Byte();   // MSB
	temp=((u16)b<<8)|a;
	if(temp&0xf800) temp=(~temp)+1;          //负温度按补码取绝对值
	*deci=(int16_t)((temp&0x07ff)*10/16);    //12位分辨率，0.0625°C/LSB
	DS18B20_Start();                          //启动下一轮转换
	return 0;
}

//总线上是否检测到器件
u8 DS18B20_Present(void)
{
	return ds18b20_present;
}
