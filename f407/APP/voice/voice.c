#include "voice.h"
#include "mic.h"
#include "usart_c3.h"
#include <string.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/* AUDIO_DATA 载荷上限 512，前 4 字节是偏移，PCM 取 506，保证每帧都是完整采样。 */
#define VOICE_CHUNK     506u
#define VOICE_MIN_MS    400u
#define VOICE_ACK_MS    300u
#define VOICE_RETRY     3u
#define VOICE_WAIT_MS   30000u
#define VOICE_RESULT_MS 5000u

/* 上传发到哪一步：0 还没发 BEGIN，1 正在发 DATA，2 正在发 END。 */
#define STEP_BEGIN 0u
#define STEP_DATA  1u
#define STEP_END   2u

static volatile u8 state = VOICE_IDLE;
static volatile u8 press_req, release_req;
static u8 step;
static u32 stamp;
static u32 total;
static u32 offset;
static u8 retries;
static u8 wait_ack;
static char text[192];
static u8 frame_seq;
static u8 payload[4 + VOICE_CHUNK]; /* Link task only, keep its stack bounded. */
static u32 upload_start, next_progress;

static const char *step_name(void)
{
    return step == STEP_BEGIN ? "BEGIN" : step == STEP_DATA ? "DATA" : "END";
}

static void set_text(const char *s)
{
    u16 i;
    /* The higher-priority UI must never see a partially written UTF-8 line. */
    taskENTER_CRITICAL();
    for(i=0; i<sizeof(text)-1 && s[i]; i++) text[i]=s[i];
    text[i]=0;
    taskEXIT_CRITICAL();
}

static void fail(const char *why)
{
	set_text(why);
	state = VOICE_FAIL;
	stamp = HAL_GetTick();
    C3_AudioSession(0);
    C3_Report();
}

void Voice_Init(void)
{
	Mic_Init();
	set_text("\xE7\xA9\xBA\xE9\x97\xB2");	/* 空闲 */
}

void Voice_Press(void)
{
    /* UI callback only posts an event; task_link owns all recording state. */
    taskENTER_CRITICAL();
    if(state != VOICE_RECORD && state != VOICE_UPLOAD && state != VOICE_WAIT)
        press_req = 1;
    taskEXIT_CRITICAL();
}

void Voice_Release(void)
{
    release_req = 1;
}

static void start_record(void)
{
    C3_AudioSession(1);
    printf("[voice] record start rate=%u max_ms=%u\r\n", MIC_RATE, MIC_MAX_MS);
    if(!Mic_Start())
    {
        printf("[voice] mic start failed err=0x%02lX\r\n", (unsigned long)Mic_Error());
        fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
        return;
    }
    state = VOICE_RECORD;
    stamp = HAL_GetTick();
    set_text("\xE5\xBD\x95\xE9\x9F\xB3");
}

static void begin_upload(void)
{
	total = Mic_Bytes();
	offset = 0;
	retries = 0;
	wait_ack = 0;
	step = STEP_BEGIN;
    upload_start = HAL_GetTick();
    next_progress = 16000u;
    printf("[voice] upload begin bytes=%lu\r\n", (unsigned long)total);
	state = VOICE_UPLOAD;
	stamp = HAL_GetTick();
	set_text("\xE4\xB8\x8A\xE4\xBC\xA0");	/* 上传 */
}

static void stop_record(void)
{
	u32 ms;
	if(state != VOICE_RECORD) return;
    ms = HAL_GetTick() - stamp;
    Mic_Stop();
    Mic_Report();
    if(Mic_Error())
    {
        printf("[voice] record failed err=0x%02lX\r\n", (unsigned long)Mic_Error());
        fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
        return;
    }
	if(ms < VOICE_MIN_MS || Mic_Bytes() < (VOICE_MIN_MS * 32u))
	{
        printf("[voice] discard short recording hold_ms=%lu\r\n", (unsigned long)ms);
        C3_AudioSession(0);
		state = VOICE_IDLE;			/* 短于 400 ms，丢弃 */
		set_text("\xE7\xA9\xBA\xE9\x97\xB2");	/* 空闲 */
		return;
	}
	begin_upload();
}

/* 发当前这一帧。只在链路任务里调用，发送本身不入队。 */
static void send_current(void)
{
    u16 len = 0;
    if(step == STEP_BEGIN)
    {
        payload[0]=(u8)total; payload[1]=(u8)(total>>8);
        payload[2]=(u8)(total>>16); payload[3]=(u8)(total>>24);
        len = 4;
    }
    else if(step == STEP_DATA)
    {
        u32 n = total - offset;
        if(n > VOICE_CHUNK) n = VOICE_CHUNK;
        payload[0]=(u8)offset; payload[1]=(u8)(offset>>8);
        payload[2]=(u8)(offset>>16); payload[3]=(u8)(offset>>24);
        memcpy(payload + 4, Mic_Data() + offset, n);
        len = (u16)(4 + n);
    }
    if(!C3_SendAudio(step, payload, len, &frame_seq, retries != 0))
    {
        printf("[voice] uart tx failed step=%s offset=%lu\r\n", step_name(), (unsigned long)offset);
        fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
        return;
    }
    wait_ack = 1;
    stamp = HAL_GetTick();
}

static void on_acked(void)
{
	retries = 0;
	wait_ack = 0;
	if(step == STEP_BEGIN)
	{
        printf("[voice] ACK BEGIN seq=%u\r\n", frame_seq);
		step = STEP_DATA;
	}
	else if(step == STEP_DATA)
	{
		u32 n = total - offset;
		if(n > VOICE_CHUNK) n = VOICE_CHUNK;
		offset += n;
        if(offset >= next_progress || offset >= total)
        {
            printf("[voice] upload %lu/%lu\r\n", (unsigned long)offset, (unsigned long)total);
            next_progress += 16000u;
        }
		if(offset >= total) step = STEP_END;
	}
	else
	{
        printf("[voice] ACK END upload_ms=%lu; wait ASR\r\n", (unsigned long)(HAL_GetTick() - upload_start));
		state = VOICE_WAIT;			/* END 已确认，等云端回答 */
		stamp = HAL_GetTick();
		set_text("\xE8\xAF\x86\xE5\x88\xAB\xE4\xB8\xAD");	/* 识别中 */
	}
}

void Voice_Tick(void)
{
    char reply[192];
    u8 got, err = 0, press, release;
    taskENTER_CRITICAL();
    press = press_req; release = release_req;
    press_req = release_req = 0;
    taskEXIT_CRITICAL();

    if(press && state != VOICE_RECORD && state != VOICE_UPLOAD && state != VOICE_WAIT)
        start_record();
    if(state == VOICE_RECORD && (release || !Mic_Busy() ||
       (HAL_GetTick() - stamp) >= MIC_MAX_MS)) stop_record();

    if(state == VOICE_UPLOAD)
    {
        if(!wait_ack) send_current();
        if(state != VOICE_UPLOAD) return;
        got = C3_TakeAck(frame_seq, &err);
        if(got == 1)
        {
            on_acked();
            /* Continue in the same tick, without another idle link period. */
            if(state == VOICE_UPLOAD) send_current();
        }
        else if(got == 2)
        {
            printf("[voice] NACK step=%s seq=%u offset=%lu code=0x%02X\r\n",
                   step_name(), frame_seq, (unsigned long)offset, err);
            fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
        }
        else if((HAL_GetTick() - stamp) >= VOICE_ACK_MS)
        {
            if(retries >= VOICE_RETRY)
            {
                printf("[voice] ACK timeout step=%s seq=%u offset=%lu retries=%u\r\n",
                       step_name(), frame_seq, (unsigned long)offset, retries);
                fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
            }
            else
            {
                retries++;
                printf("[voice] retry step=%s seq=%u offset=%lu attempt=%u\r\n",
                       step_name(), frame_seq, (unsigned long)offset, retries);
                send_current();
            }
        }
    }

    if(state == VOICE_WAIT)
    {
        got = C3_TakeAck(frame_seq, &err);
        if(got == 2)
        {
            printf("[voice] ASR NACK code=0x%02X\r\n", err);
            fail("\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA4\xB1\xE8\xB4\xA5");
        }
        else if(C3_TakeAsr(reply, sizeof(reply)))
        {
            set_text(reply[0] ? reply : "\xE6\xB2\xA1\xE5\x90\xAC\xE6\xB8\x85");
            printf("[voice] ASR_TEXT=%s\r\n", reply);
            state = VOICE_RESULT;
            stamp = HAL_GetTick();
            C3_AudioSession(0);
            C3_Report();
        }
        else if((HAL_GetTick() - stamp) >= VOICE_WAIT_MS)
        {
            printf("[voice] ASR timeout ms=%u\r\n", VOICE_WAIT_MS);
            fail("\xE8\xAF\x86\xE5\x88\xAB\xE8\xB6\x85\xE6\x97\xB6");
        }
    }

    if((state == VOICE_RESULT || state == VOICE_FAIL) &&
       (HAL_GetTick() - stamp) >= VOICE_RESULT_MS)
    {
        state = VOICE_IDLE;
        set_text("\xE7\xA9\xBA\xE9\x97\xB2");
    }
}

u8 Voice_State(void)
{
	return state;
}

const char *Voice_Text(void)
{
	return text;
}
