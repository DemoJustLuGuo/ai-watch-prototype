#ifndef _pedometer_H
#define _pedometer_H

#include "system.h"

/* 计步。合加速度减去重力基线，动态量越过阈值后回落记一步。
   阈值和步间隔可在运行时改，断电由 EEPROM 记住。
   跌倒粗判共用同一次合加速度，不单独再算。 */

#define PEDO_TH_MIN		50u		/* 0.001 g */
#define PEDO_TH_MAX		500u
#define PEDO_TH_STEP	10u

void Pedometer_Init(void);

/* 喂一次 20 ms 采样的三轴原始值（±2 g，16384 LSB/g）。
   dyn_mg 返回本次动态量（0.001 g，负值记 0），fall 在判定为跌倒时置 1。
   两个输出指针都可以传 NULL。返回当前总步数。 */
u32 Pedometer_Feed(short ax, short ay, short az, u16 *dyn_mg, u8 *fall);

u32 Pedometer_Steps(void);
void Pedometer_SetSteps(u32 steps);

void Pedometer_SetThreshold(u16 milli_g, u16 gap_ms);
u16 Pedometer_Threshold(void);
u16 Pedometer_Gap(void);

/* 步间隔只在这四档里循环，界面按键不用自己管范围。 */
u16 Pedometer_NextGap(u16 gap_ms);

#endif
