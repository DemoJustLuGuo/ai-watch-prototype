#ifndef _voice_H
#define _voice_H

#include "system.h"

/* 方案 4.3 的语音状态机。按住说话、松开停止，短于 400 ms 丢弃，
   最长 10 秒。三段帧发给 C3，每帧等 ACK，超时 300 ms 重发，最多 3 次。
   回来的 ASR_TEXT 原样显示，不做关键词匹配。 */

#define VOICE_IDLE      0u
#define VOICE_RECORD    1u
#define VOICE_UPLOAD    2u
#define VOICE_WAIT      3u	/* 等 C3 把音频交给云端，超时 8 s */
#define VOICE_RESULT    4u
#define VOICE_FAIL      5u

void Voice_Init(void);
void Voice_Press(void);		/* UI 只投递按下事件，由链路任务启动录音 */
void Voice_Release(void);	/* 投递松开事件，下一次 Voice_Tick 停止 */
void Voice_Tick(void);		/* 链路任务里调用，推进上传和超时 */

u8 Voice_State(void);
const char *Voice_Text(void);	/* 结果或失败原因，UTF-8 */

#endif
