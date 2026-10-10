#ifndef __MPU6050_H
#define __MPU6050_H
#include "iic.h"

/* 方案 4.2 用到的寄存器，其余省略 */
#define MPU6050_SAMPLE_RATE_REG		0X19	//采样率分频寄存器
#define MPU6050_CFG_REG				0X1A	//配置寄存器
#define MPU6050_GYRO_CFG_REG		0X1B	//陀螺仪配置寄存器
#define MPU6050_ACCEL_CFG_REG	0X1C	//加速度计配置寄存器
#define MPU6050_ACCEL_XOUTH_REG	0X3B	//加速度值,X轴高8位寄存器
#define MPU6050_INT_EN_REG			0X38	//中断使能寄存器
#define MPU6050_INTBP_CFG_REG		0X37	//中断/旁路设置寄存器
#define MPU6050_USER_CTRL_REG		0X6A	//用户控制寄存器
#define MPU6050_PWR_MGMT1_REG		0X6B	//电源管理寄存器1
#define MPU6050_PWR_MGMT2_REG		0X6C	//电源管理寄存器2
#define MPU6050_DEVICE_ID_REG		0X75	//器件ID寄存器

//板载 AD0 接地，7 位地址 0x68
#define MPU6050_ADDR				0X68

u8 MPU6050_Init(void); 								//初始化MPU6050
u8 MPU6050_Read_Len(u8 addr,u8 reg,u8 len,u8 *buf); //IIC连续读
u8 MPU6050_Write_Byte(u8 reg,u8 data);				//IIC写一个字节
u8 MPU6050_Get_Accelerometer(short *ax,short *ay,short *az);

#endif
