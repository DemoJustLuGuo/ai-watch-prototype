#include "mpu6050.h"
#include "SysTick.h"

/* 只做计步需要的部分（方案 4.2）：±2g、50Hz、DLPF 44Hz，不用 DMP。
   初始化序列按普中例程 37，PWR_MGMT1 时钟源按方案取内部 8MHz（0x00）。 */

static u8 MPU6050_Read_Byte_Reg(u8 reg);

//初始化MPU6050
//返回值:0,成功
//    其他,错误代码
u8 MPU6050_Init(void)
{
	u8 res;
	IIC_Init();//初始化IIC接口
	MPU6050_Write_Byte(MPU6050_PWR_MGMT1_REG,0X80);	//复位MPU6050
    delay_ms(100);
	MPU6050_Write_Byte(MPU6050_PWR_MGMT1_REG,0X00);	//唤醒，时钟用内部
	MPU6050_Write_Byte(MPU6050_GYRO_CFG_REG,0x00);	//陀螺仪量程,±250dps
	MPU6050_Write_Byte(MPU6050_ACCEL_CFG_REG,0x00);	//加速度计量程,±2g
	MPU6050_Write_Byte(MPU6050_SAMPLE_RATE_REG,19);	//采样率 1kHz/(1+19)=50Hz
	MPU6050_Write_Byte(MPU6050_CFG_REG,0x03);		//DLPF 约44Hz
	MPU6050_Write_Byte(MPU6050_INT_EN_REG,0X00);	//关闭所有中断
	MPU6050_Write_Byte(MPU6050_USER_CTRL_REG,0X00);	//I2C主模式关闭
	MPU6050_Write_Byte(MPU6050_INTBP_CFG_REG,0X80);	//INT引脚低电平有效
	res=MPU6050_Read_Byte_Reg(MPU6050_DEVICE_ID_REG);
	if(res==MPU6050_ADDR)//器件ID正确
	{
		MPU6050_Write_Byte(MPU6050_PWR_MGMT2_REG,0X00);	//加速度与角陀螺仪都工作
	}else return 1;
	return 0;
}

//IIC读一个字节
//reg:寄存器地址
//返回值:读到的数据
static u8 MPU6050_Read_Byte_Reg(u8 reg)
{
	u8 res;
    IIC_Start();
	IIC_Send_Byte((MPU6050_ADDR<<1)|0);//发送器件地址+写命令
	IIC_Wait_Ack();		//等待应答
    IIC_Send_Byte(reg);	//写寄存器地址
    IIC_Wait_Ack();		//等待应答
    IIC_Start();
	IIC_Send_Byte((MPU6050_ADDR<<1)|1);//发送器件地址+读命令
    IIC_Wait_Ack();		//等待应答
	res=IIC_Read_Byte(0);//读取数据,发送nACK
    IIC_Stop();			//发送一个停止条件
	return res;
}

//IIC连续读
//addr:器件地址;reg:要读取的寄存器地址;len:要读取的长度;buf:读取到的数据存储区
//返回值:0,正常;其他,错误代码
u8 MPU6050_Read_Len(u8 addr,u8 reg,u8 len,u8 *buf)
{
 	IIC_Start();
	IIC_Send_Byte((addr<<1)|0);//发送器件地址+写命令
	if(IIC_Wait_Ack())	//等待应答
	{
		IIC_Stop();
		return 1;
	}
    IIC_Send_Byte(reg);	//写寄存器地址
    IIC_Wait_Ack();		//等待应答
    IIC_Start();
	IIC_Send_Byte((addr<<1)|1);//发送器件地址+读命令
    IIC_Wait_Ack();		//等待应答
	while(len)
	{
		if(len==1)*buf=IIC_Read_Byte(0);//最后一个字节,发送nACK
		else *buf=IIC_Read_Byte(1);		//否则发送ACK
		len--;
		buf++;
	}
    IIC_Stop();	//发送一个停止条件
	return 0;
}

//IIC写一个字节
//reg:寄存器地址;data:数据
//返回值:0,正常;其他,错误代码
u8 MPU6050_Write_Byte(u8 reg,u8 data)
{
    IIC_Start();
	IIC_Send_Byte((MPU6050_ADDR<<1)|0);//发送器件地址+写命令
	if(IIC_Wait_Ack())	//等待应答
	{
		IIC_Stop();
		return 1;
	}
    IIC_Send_Byte(reg);	//写寄存器地址
    IIC_Wait_Ack();		//等待应答
	IIC_Send_Byte(data);//发送数据
	if(IIC_Wait_Ack())	//等待ACK
	{
		IIC_Stop();
		return 1;
	}
    IIC_Stop();
	return 0;
}

//得到加速度值(原始值)
//ax,ay,az:x,y,z轴的原始读数(带输出)
//返回值:0,成功;其他,错误代码
u8 MPU6050_Get_Accelerometer(short *ax,short *ay,short *az)
{
    u8 buf[6],res;
	res=MPU6050_Read_Len(MPU6050_ADDR,MPU6050_ACCEL_XOUTH_REG,6,buf);
	if(res==0)
	{
		*ax=((u16)buf[0]<<8)|buf[1];
		*ay=((u16)buf[2]<<8)|buf[3];
		*az=((u16)buf[4]<<8)|buf[5];
	}
    return res;
}
