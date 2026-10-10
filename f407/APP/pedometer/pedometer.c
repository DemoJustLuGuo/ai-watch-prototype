#include "pedometer.h"
#include <math.h>

/* 慢速基线 base = 0.98*base + 0.02*mag，初值 1.0 g。
   dyn = mag - base，从阈值之上回落且距上一步够久，记一步。 */

#define ACC_LSB_PER_G	16384.0f
#define SAMPLE_MS		20u

/* 跌倒粗判，与计步阈值分开，不进校准页。 */
#define FALL_HIGH_G		2.0f	/* 冲击 */
#define FALL_LOW_G		0.4f	/* 失重 */
#define FALL_LOW_MS		80u		/* 失重要持续这么久才算数 */
#define FALL_ARM_MS		1000u	/* 失重之后这么久内的冲击也算跌倒 */
#define FALL_LOCK_MS	5000u	/* 触发后静默，避免连报 */

static float base;
static u8    above;
static u32   step_count;
static u32   last_step_ms;
static u32   now_ms;

static float th_dyn;		/* g */
static u32   step_gap_ms;

static u8  fall_high_cnt;	/* 连续超过 FALL_HIGH_G 的采样数 */
static u32 fall_low_ms;		/* 连续低于 FALL_LOW_G 的时长 */
static u32 fall_armed_until;	/* 失重成立后，冲击有效到这个时刻 */
static u32 fall_lock_until;

void Pedometer_Init(void)
{
	base = 1.0f;
	above = 0;
	step_count = 0;
	last_step_ms = 0;
	now_ms = 0;
	th_dyn = 0.15f;
	step_gap_ms = 300u;
	fall_high_cnt = 0;
	fall_low_ms = 0;
	fall_armed_until = 0;
	fall_lock_until = 0;
}

void Pedometer_SetThreshold(u16 milli_g, u16 gap_ms)
{
	if(milli_g < PEDO_TH_MIN) milli_g = PEDO_TH_MIN;
	if(milli_g > PEDO_TH_MAX) milli_g = PEDO_TH_MAX;
	th_dyn = (float)milli_g / 1000.0f;
	step_gap_ms = gap_ms;
}

u16 Pedometer_Threshold(void)
{
	return (u16)(th_dyn * 1000.0f + 0.5f);
}

u16 Pedometer_Gap(void)
{
	return (u16)step_gap_ms;
}

u16 Pedometer_NextGap(u16 gap_ms)
{
	if(gap_ms < 300) return 300;
	if(gap_ms < 400) return 400;
	if(gap_ms < 500) return 500;
	return 200;
}

u32 Pedometer_Feed(short ax, short ay, short az, u16 *dyn_mg, u8 *fall)
{
	float gx = ax / ACC_LSB_PER_G;
	float gy = ay / ACC_LSB_PER_G;
	float gz = az / ACC_LSB_PER_G;
	float mag = sqrtf(gx * gx + gy * gy + gz * gz);
	float dyn;
	float amag = mag < 0 ? -mag : mag;

	now_ms += SAMPLE_MS;
	base = 0.98f * base + 0.02f * mag;
	dyn = mag - base;

	if(dyn_mg) *dyn_mg = dyn > 0 ? (u16)(dyn * 1000.0f) : 0;
	if(fall) *fall = 0;

	if(dyn > th_dyn)
	{
		above = 1;
	}
	else if(above)
	{
		above = 0;
		if((now_ms - last_step_ms) >= step_gap_ms)
		{
			step_count++;
			last_step_ms = now_ms;
		}
	}

	/* 跌倒：连续两次冲击，或一段失重之后 1 秒内的冲击。 */
	if(amag > FALL_HIGH_G) fall_high_cnt++;
	else fall_high_cnt = 0;

	if(amag < FALL_LOW_G)
	{
		fall_low_ms += SAMPLE_MS;
		if(fall_low_ms >= FALL_LOW_MS) fall_armed_until = now_ms + FALL_ARM_MS;
	}
	else
	{
		fall_low_ms = 0;
	}

	if(fall && now_ms >= fall_lock_until && fall_high_cnt >= 2 &&
	   (fall_high_cnt == 2 || now_ms < fall_armed_until))
	{
		*fall = 1;
		fall_lock_until = now_ms + FALL_LOCK_MS;
	}
	return step_count;
}

u32 Pedometer_Steps(void)
{
	return step_count;
}

void Pedometer_SetSteps(u32 steps)
{
	step_count = steps;
}
