/* 节选自 f407/APP/voice/voice.c —— 上传停等 ACK 与识别等待状态机 */

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
}
