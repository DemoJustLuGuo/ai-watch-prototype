/* 节选自 f407/APP/pedometer/pedometer.c —— 计步与跌倒粗判核心 */

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
	/* ……（fall_high_cnt/fall_armed_until 命中后置 *fall 并进入 FALL_LOCK_MS 静默，略） */
	return step_count;
}
