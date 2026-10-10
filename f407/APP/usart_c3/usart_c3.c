#include "usart_c3.h"
#include "proto.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

/* F407 与 ESP32-C3 走 UART4（PC10=TX/AF8，PC11=RX/AF8），460800 8N1。
   发送只在链路任务里做：中断和其他任务都只往队列里丢，避免在中断或
   LVGL 回调里阻塞 100 ms。 */

#define C3_Q_TEXT 2
#define C3_Q_TX   4
#define C3_CITY_MAX 16			//城市名 UTF-8，最长"重庆"=6 字节，留余量

typedef struct { u8 type; u32 arg; u16 len; char city[C3_CITY_MAX]; const u8 *data; } c3_tx_t;
typedef struct { u16 len; char text[192]; } c3_text_t;	//天气行约 40 字节，ASR 回答约 150 字节

static UART_HandleTypeDef UART4_Handler;
static QueueHandle_t q_tx;
static QueueHandle_t q_weather;
static QueueHandle_t q_asr;

static volatile u8 online=0;
static u8 tx_seq=0;
static QueueHandle_t q_ack;
typedef struct { u8 kind, seq, error; } c3_ack_t;
static u8 audio_active;
static u8 tx_frame[PROTO_MAX_FRAME]; /* Only task_link transmits. */
static volatile u32 rx_crc, rx_ore, rx_drop;
static u32 stale_ack;

static u8 c3_send_seq(u8 type, u8 seq, const u8 *payload, u16 len)
{
    u16 n = proto_build(tx_frame, type, seq, payload, len);
    return HAL_UART_Transmit(&UART4_Handler, tx_frame, n, 100) == HAL_OK;
}

static void c3_send(u8 type, const u8 *payload, u16 len)
{
    (void)c3_send_seq(type, tx_seq++, payload, len);
}

void C3_Init(u32 bound)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	q_tx=xQueueCreate(C3_Q_TX, sizeof(c3_tx_t));
	q_weather=xQueueCreate(C3_Q_TEXT, sizeof(c3_text_t));
	q_asr=xQueueCreate(C3_Q_TEXT, sizeof(c3_text_t));
	q_ack=xQueueCreate(4, sizeof(c3_ack_t));

	//时钟：PC10=UART4_TX(AF8)，PC11=UART4_RX(AF8)
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_UART4_CLK_ENABLE();

	GPIO_InitStructure.Pin=GPIO_PIN_10|GPIO_PIN_11;
	GPIO_InitStructure.Mode=GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull=GPIO_PULLUP;
	GPIO_InitStructure.Speed=GPIO_SPEED_FAST;
	GPIO_InitStructure.Alternate=GPIO_AF8_UART4;
	HAL_GPIO_Init(GPIOC,&GPIO_InitStructure);

	UART4_Handler.Instance=UART4;
	UART4_Handler.Init.BaudRate=bound;
	UART4_Handler.Init.WordLength=UART_WORDLENGTH_8B;
	UART4_Handler.Init.StopBits=UART_STOPBITS_1;
	UART4_Handler.Init.Parity=UART_PARITY_NONE;
	UART4_Handler.Init.HwFlowCtl=UART_HWCONTROL_NONE;
	UART4_Handler.Init.Mode=UART_MODE_TX_RX;
	HAL_UART_Init(&UART4_Handler);	//会调用 Public 里那个只认 USART1 的 MspInit，对 UART4 空转

	//优先级必须落在 FreeRTOS 可调用区（数值 >= 5），中断里要唤醒链路任务
	HAL_NVIC_SetPriority(UART4_IRQn,6,0);
	HAL_NVIC_EnableIRQ(UART4_IRQn);
	__HAL_UART_ENABLE_IT(&UART4_Handler,UART_IT_RXNE);
}

//收到整帧后的处理（在中断上下文，只入队）
static void on_frame(const proto_frame_t *f)
{
	BaseType_t woken=pdFALSE;
	c3_text_t m;

    if(f->src != PROTO_SRC_C3 || f->dst != PROTO_SRC_F4) return;
	switch(f->type)
	{
	case PT_HELLO_ACK:
		if(f->len>=1) online=(f->payload[0]==1);
		break;
    case PT_ACK:
    case PT_NACK:
        {
            c3_ack_t a;
            a.kind = (f->type == PT_ACK) ? 1u : 2u;
            a.seq = f->seq;
            a.error = f->len ? f->payload[0] : 0;
            if(xQueueSendFromISR(q_ack, &a, &woken) != pdTRUE) rx_drop++;
        }
        break;
	case PT_WEATHER_RESULT:
	case PT_ASR_TEXT:
		m.len=f->len;
		if(m.len>sizeof(m.text)-1) m.len=sizeof(m.text)-1;
		memcpy(m.text,f->payload,m.len);
		m.text[m.len]=0;
        if(xQueueSendFromISR(f->type==PT_WEATHER_RESULT?q_weather:q_asr, &m, &woken) != pdTRUE) rx_drop++;
		break;
	default:
		break;
	}
	portYIELD_FROM_ISR(woken);
}

void UART4_IRQHandler(void)
{
	static proto_frame_t f;
	u8 crc_err;
	u8 b;

	if(__HAL_UART_GET_FLAG(&UART4_Handler,UART_FLAG_ORE))	//溢出：读 DR 清标志
	{
        __HAL_UART_CLEAR_OREFLAG(&UART4_Handler);
        rx_ore++;
		return;
	}
	if(__HAL_UART_GET_FLAG(&UART4_Handler,UART_FLAG_RXNE))
	{
		b=(u8)(UART4_Handler.Instance->DR & 0xFF);	//读 DR 自动清 RXNE
		if(proto_feed(b,&f,&crc_err))
		{
			on_frame(&f);
		}
        else if(crc_err) rx_crc++;
	}
}

//链路任务调用：未连上时每 2s 补一帧 HELLO，并发送其他任务投递的请求
void C3_Tick(void)
{
	static u32 last_hello_ms=0;
	c3_tx_t req;
	u32 now=HAL_GetTick();
    static u8 last_online = 0xFF;
    if(last_online != online)
    {
        last_online = online;
        printf("[c3] online=%u\r\n", online);
    }
    /* C3 weather requests block its link task. Defer them during audio. */
    if(audio_active) return;

	if(!online && (now-last_hello_ms)>=2000u)
	{
		last_hello_ms=now;
		c3_send(PT_HELLO, 0, 0);
	}
	while(xQueueReceive(q_tx, &req, 0)==pdTRUE)
	{
		if(req.type==PT_CMD_WEATHER)
		{
			/* 连按会积压多次请求。C3 一次查询要几秒，只发最后一次，
			   前面的城市结果到了也用不上。 */
			{
				c3_tx_t newer;
				while(xQueuePeek(q_tx, &newer, 0)==pdTRUE && newer.type==PT_CMD_WEATHER)
					xQueueReceive(q_tx, &req, 0);
			}
			{
				u16 n=0;
				while(req.city[n] && n<C3_CITY_MAX) n++;	/* 无结尾 0 也可，C3 按长度读 */
				c3_send(PT_CMD_WEATHER, n?(const u8 *)req.city:0, n);
			}
		}
		else if(req.type==PT_ALERT_FALL)
		{
			u8 p[4];
			p[0]=(u8)(req.arg&0xFF); p[1]=(u8)(req.arg>>8);
			p[2]=(u8)(req.arg>>16);  p[3]=(u8)(req.arg>>24);
			c3_send(PT_ALERT_FALL, p, 4);
		}
		else if(req.type==PT_AUDIO_BEGIN || req.type==PT_AUDIO_DATA || req.type==PT_AUDIO_END)
		{
			c3_send(req.type, req.data, req.len);
		}
	}
}

u8 C3_IsOnline(void)
{
	return online;
}

static u8 take_text(QueueHandle_t q, char *out, u16 cap)
{
	c3_text_t m;
	if(xQueueReceive(q, &m, 0)!=pdTRUE) return 0;
	if(cap==0) return 1;
	if(m.len>cap-1) m.len=cap-1;
	memcpy(out, m.text, m.len);
	out[m.len]=0;
	return 1;
}

u8 C3_TakeWeather(char *out, u16 cap)
{
	return take_text(q_weather, out, cap);
}

u8 C3_TakeAsr(char *out, u16 cap)
{
	return take_text(q_asr, out, cap);
}

//从任务上下文调用：只入队，真正发送在 C3_Tick 里
static void enqueue(u8 type, u32 arg, const char *city)
{
	c3_tx_t req;
	u16 i;
	req.type=type; req.arg=arg;
	for(i=0;i<C3_CITY_MAX;i++) req.city[i]=0;
	if(city) for(i=0;i<C3_CITY_MAX-1 && city[i];i++) req.city[i]=city[i];
	if(xPortIsInsideInterrupt())
	{
		BaseType_t woken=pdFALSE;
		xQueueSendFromISR(q_tx, &req, &woken);
		portYIELD_FROM_ISR(woken);
	}
	else xQueueSend(q_tx, &req, 0);
}

void C3_RequestWeather(const char *city)
{
	enqueue(PT_CMD_WEATHER, 0, city);
}

void C3_SendFallAlert(u32 unix_or_hms)
{
	enqueue(PT_ALERT_FALL, unix_or_hms, 0);
}

/* New frame allocates a SEQ. A timeout resends exactly the same SEQ. */
u8 C3_SendAudio(u8 phase, const u8 *payload, u16 len, u8 *seq, u8 retry)
{
    u8 type = (phase == 0) ? PT_AUDIO_BEGIN : (phase == 1) ? PT_AUDIO_DATA : PT_AUDIO_END;
    if(!retry) *seq = tx_seq++;
    return c3_send_seq(type, *seq, payload, len);
}

u8 C3_TakeAck(u8 seq, u8 *err)
{
    c3_ack_t a;
    while(xQueueReceive(q_ack, &a, 0) == pdTRUE)
    {
        if(a.kind == 1 && a.seq != seq) { stale_ack++; continue; }
        /* Current C3 uses its own SEQ for NACK, unlike ACK. Keep compatibility. */
        *err = a.error;
        return a.kind;
    }
    return 0;
}

void C3_AudioSession(u8 active)
{
    audio_active = active;
    if(active)
    {
        xQueueReset(q_ack);
        xQueueReset(q_asr);
    }
}

void C3_Report(void)
{
    printf("[c3] online=%u crc=%lu ore=%lu drop=%lu stale_ack=%lu\r\n",
           online, (unsigned long)rx_crc, (unsigned long)rx_ore,
           (unsigned long)rx_drop, (unsigned long)stale_ack);
}
