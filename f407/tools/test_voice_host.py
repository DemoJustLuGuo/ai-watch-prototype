"""Run the actual mic/voice/C3/protocol C sources against a simulated HAL.

Checks sample order/tails and stop-and-wait framing, not real ADC timing,
electrical levels, UART hardware, SRAM timing, or cloud recognition.
Generated C fixtures/executable stay under build/voice_host.
"""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/voice_host"
OUT.mkdir(parents=True, exist_ok=True)

SYSTEM = r"""
#ifndef TEST_SYSTEM_H
#define TEST_SYSTEM_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u32 NDTR, CR, SR, DR, LISR, CNT; } Reg;
typedef struct {
 u32 Channel,Direction,PeriphInc,MemInc,PeriphDataAlignment,MemDataAlignment,
 Mode,Priority,FIFOMode,FIFOThreshold,MemBurst,PeriphBurst,ClockPrescaler,
 Resolution,ScanConvMode,ContinuousConvMode,DiscontinuousConvMode,
 ExternalTrigConvEdge,ExternalTrigConv,DataAlign,NbrOfConversion,
 DMAContinuousRequests,EOCSelection,Prescaler,Period,CounterMode,ClockDivision,
 BaudRate,WordLength,StopBits,Parity,HwFlowCtl;
} Init;
typedef struct { Reg *Instance; Init Init; u32 State, ErrorCode; } DMA_HandleTypeDef;
typedef struct { Reg *Instance; Init Init; u32 State, ErrorCode; DMA_HandleTypeDef *DMA_Handle; } ADC_HandleTypeDef;
typedef struct { Reg *Instance; Init Init; u32 State; } TIM_HandleTypeDef;
typedef struct { Reg *Instance; Init Init; } UART_HandleTypeDef;
typedef struct { u32 Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { u32 Channel, Rank, SamplingTime; } ADC_ChannelConfTypeDef;
typedef struct { u32 MasterOutputTrigger, MasterSlaveMode; } TIM_MasterConfigTypeDef;
extern Reg reg_adc, reg_tim, reg_dma, reg_dma2, reg_uart;
#define ADC1 (&reg_adc)
#define TIM3 (&reg_tim)
#define DMA2_Stream0 (&reg_dma)
#define DMA2 (&reg_dma2)
#define UART4 (&reg_uart)
#define GPIOA ((void*)0)
#define GPIOC ((void*)0)
#define HAL_OK 0
#define HAL_DMA_STATE_BUSY 1
#define HAL_ADC_ERROR_DMA 2
#define DMA_LISR_HTIF0 1
#define DMA_LISR_TCIF0 2
#define DMA_LISR_TEIF0 4
#define DMA_LISR_DMEIF0 8
#define DMA_LISR_FEIF0 16
#define ADC_FLAG_OVR 32
#define UART_FLAG_ORE 64
#define UART_FLAG_RXNE 128
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_DMA2_CLK_ENABLE() ((void)0)
#define __HAL_RCC_UART4_CLK_ENABLE() ((void)0)
#define __HAL_TIM_DISABLE(h) ((h)->Instance->CR = 0)
#define __HAL_ADC_DISABLE(h) ((h)->Instance->CR = 0)
#define __HAL_DMA_DISABLE(h) ((h)->Instance->CR = 0)
#define __HAL_TIM_SET_COUNTER(h,n) ((h)->Instance->CNT = (n))
#define __HAL_TIM_CLEAR_FLAG(h,n) ((void)0)
#define __HAL_LINKDMA(h,field,d) ((h)->field = &(d))
#define __HAL_ADC_GET_FLAG(h,f) ((h)->Instance->SR & (f))
#define __HAL_UART_GET_FLAG(h,f) ((h)->Instance->SR & (f))
#define __HAL_UART_CLEAR_OREFLAG(h) ((h)->Instance->SR = 0)
#define __HAL_UART_ENABLE_IT(h,f) ((void)0)
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
int HAL_DMA_Init(DMA_HandleTypeDef *);
int HAL_ADC_Init(ADC_HandleTypeDef *);
int HAL_ADC_ConfigChannel(ADC_HandleTypeDef *, ADC_ChannelConfTypeDef *);
int HAL_TIM_Base_Init(TIM_HandleTypeDef *);
int HAL_TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef *, TIM_MasterConfigTypeDef *);
int HAL_ADC_Start_DMA(ADC_HandleTypeDef *, uint32_t *, uint32_t);
int HAL_ADC_Stop_DMA(ADC_HandleTypeDef *);
int HAL_TIM_Base_Start(TIM_HandleTypeDef *);
int HAL_TIM_Base_Stop(TIM_HandleTypeDef *);
int HAL_UART_Init(UART_HandleTypeDef *);
int HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
void HAL_NVIC_SetPriority(int,int,int);
void HAL_NVIC_EnableIRQ(int);
void HAL_NVIC_DisableIRQ(int);
void HAL_NVIC_ClearPendingIRQ(int);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *);
void HAL_ADC_IRQHandler(ADC_HandleTypeDef *);
uint32_t HAL_GetTick(void);
#endif
"""
constants = """GPIO_PIN_5 GPIO_MODE_ANALOG GPIO_NOPULL DMA_CHANNEL_0
DMA_PERIPH_TO_MEMORY DMA_PINC_DISABLE DMA_MINC_ENABLE DMA_PDATAALIGN_HALFWORD
DMA_MDATAALIGN_HALFWORD DMA_CIRCULAR DMA_PRIORITY_HIGH DMA_FIFOMODE_DISABLE
DMA_FIFO_THRESHOLD_FULL DMA_MBURST_SINGLE DMA_PBURST_SINGLE ADC_CLOCK_SYNC_PCLK_DIV4
ADC_RESOLUTION_12B DISABLE ENABLE ADC_EXTERNALTRIGCONVEDGE_RISING
ADC_EXTERNALTRIGCONV_T3_TRGO ADC_DATAALIGN_RIGHT ADC_EOC_SINGLE_CONV ADC_CHANNEL_5
ADC_SAMPLETIME_84CYCLES TIM_COUNTERMODE_UP TIM_CLOCKDIVISION_DIV1 TIM_TRGO_UPDATE
TIM_MASTERSLAVEMODE_DISABLE DMA2_Stream0_IRQn ADC_IRQn TIM_FLAG_UPDATE
GPIO_PIN_10 GPIO_PIN_11 GPIO_MODE_AF_PP GPIO_PULLUP GPIO_SPEED_FAST GPIO_AF8_UART4
UART_WORDLENGTH_8B UART_STOPBITS_1 UART_PARITY_NONE UART_HWCONTROL_NONE
UART_MODE_TX_RX UART4_IRQn UART_IT_RXNE"""
SYSTEM += "\n".join(f"#define {name} {i+1}" for i, name in enumerate(constants.split()))
(OUT / "system.h").write_text(SYSTEM, encoding="utf-8")

RTOS = r"""
#ifndef TEST_RTOS_H
#define TEST_RTOS_H
#include <stddef.h>
typedef int BaseType_t;
typedef struct TestQueue *QueueHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define portYIELD_FROM_ISR(x) ((void)(x))
#define xPortIsInsideInterrupt() 0
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueSend(QueueHandle_t,const void*,unsigned);
int xQueueSendFromISR(QueueHandle_t,const void*,BaseType_t*);
int xQueueReceive(QueueHandle_t,void*,unsigned);
int xQueuePeek(QueueHandle_t,void*,unsigned);
void xQueueReset(QueueHandle_t);
#endif
"""
for header in ("FreeRTOS.h", "task.h", "queue.h"):
    (OUT / header).write_text(RTOS, encoding="utf-8")

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "system.h"
#include "FreeRTOS.h"
#include "../../APP/mic/mic.c"
#include "../../APP/proto/proto.c"
#include "../../APP/usart_c3/usart_c3.c"
#include "../../APP/voice/voice.c"

Reg reg_adc, reg_tim, reg_dma, reg_dma2, reg_uart;
static u32 ticks, sample_index;
static int start_fault, tx_fault;
typedef struct { u16 len; u8 data[PROTO_MAX_FRAME]; } Sent;
static Sent sent[(MIC_MAX_BYTES + VOICE_CHUNK - 1u) / VOICE_CHUNK + 10u];
static unsigned sent_count;
struct TestQueue { unsigned cap, size, count; u8 data[4096]; };
QueueHandle_t xQueueCreate(unsigned cap,unsigned size) {
 QueueHandle_t q=calloc(1,sizeof(*q)); q->cap=cap; q->size=size; return q;
}
int xQueueSend(QueueHandle_t q,const void*p,unsigned wait) {
 (void)wait; if(q->count==q->cap) return 0;
 memcpy(q->data+q->count++*q->size,p,q->size); return 1;
}
int xQueueSendFromISR(QueueHandle_t q,const void*p,BaseType_t*w) { (void)w; return xQueueSend(q,p,0); }
int xQueuePeek(QueueHandle_t q,void*p,unsigned wait) {
 (void)wait; if(!q->count) return 0; memcpy(p,q->data,q->size); return 1;
}
int xQueueReceive(QueueHandle_t q,void*p,unsigned wait) {
 if(!xQueuePeek(q,p,wait)) return 0;
 q->count--;
 memmove(q->data,q->data+q->size,q->count*q->size); return 1;
}
void xQueueReset(QueueHandle_t q) { q->count=0; }
void HAL_GPIO_Init(void*p,GPIO_InitTypeDef*g) { (void)p;(void)g; }
int HAL_DMA_Init(DMA_HandleTypeDef*d) { d->State=0; memset(&reg_dma,0,sizeof(reg_dma)); reg_dma2.LISR=0; return 0; }
int HAL_ADC_Init(ADC_HandleTypeDef*a) { memset(&reg_adc,0,sizeof(reg_adc)); a->ErrorCode=0; return 0; }
int HAL_ADC_ConfigChannel(ADC_HandleTypeDef*a,ADC_ChannelConfTypeDef*c) { (void)a;(void)c;return 0; }
int HAL_TIM_Base_Init(TIM_HandleTypeDef*t) { (void)t;return 0; }
int HAL_TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef*t,TIM_MasterConfigTypeDef*m) { (void)t;(void)m;return 0; }
int HAL_TIM_Base_Start(TIM_HandleTypeDef*t) { t->Instance->CR=1;return 0; }
int HAL_TIM_Base_Stop(TIM_HandleTypeDef*t) { t->Instance->CR=0;return 0; }
int HAL_ADC_Start_DMA(ADC_HandleTypeDef*a,uint32_t*p,uint32_t n) {
 (void)p; if(start_fault) return 1;
 a->DMA_Handle->Instance->NDTR=n; a->DMA_Handle->State=HAL_DMA_STATE_BUSY;
 a->Instance->CR=1; a->DMA_Handle->Instance->CR=1; sample_index=0; return 0;
}
int HAL_ADC_Stop_DMA(ADC_HandleTypeDef*a) {
 a->Instance->CR=0; a->DMA_Handle->Instance->CR=0;
 a->DMA_Handle->State=0; reg_dma2.LISR=0; return 0;
}
int HAL_UART_Init(UART_HandleTypeDef*u) { (void)u; return 0; }
int HAL_UART_Transmit(UART_HandleTypeDef*u,uint8_t*p,uint16_t n,uint32_t wait) {
 (void)u;(void)wait; if(tx_fault) return 1;
 assert(sent_count<sizeof(sent)/sizeof(sent[0]) && n<=PROTO_MAX_FRAME);
 sent[sent_count].len=n; memcpy(sent[sent_count++].data,p,n); return 0;
}
void HAL_NVIC_SetPriority(int a,int b,int c) { (void)a;(void)b;(void)c; }
void HAL_NVIC_EnableIRQ(int a) { (void)a; }
void HAL_NVIC_DisableIRQ(int a) { (void)a; }
void HAL_NVIC_ClearPendingIRQ(int a) { (void)a; }
void HAL_ADC_IRQHandler(ADC_HandleTypeDef*a) { HAL_ADC_ErrorCallback(a); }
void HAL_DMA_IRQHandler(DMA_HandleTypeDef*d) {
 (void)d;
 if(reg_dma2.LISR & DMA_LISR_HTIF0) { reg_dma2.LISR &= ~DMA_LISR_HTIF0; HAL_ADC_ConvHalfCpltCallback(&hadc); }
 if(reg_dma2.LISR & DMA_LISR_TCIF0) { reg_dma2.LISR &= ~DMA_LISR_TCIF0; HAL_ADC_ConvCpltCallback(&hadc); }
}
uint32_t HAL_GetTick(void) { return ticks; }

static u16 sample(u32 i) { return (u16)((i*13u+29u)&4095u); }
static void feed(u32 n,int irq) {
 u32 i;
 for(i=0;i<n;i++) {
  if(!reg_dma.CR) break;
  raw[MIC_RING-reg_dma.NDTR]=sample(sample_index++);
  reg_dma.NDTR--;
  if(reg_dma.NDTR==MIC_BLK) reg_dma2.LISR |= DMA_LISR_HTIF0;
  if(reg_dma.NDTR==0) { reg_dma.NDTR=MIC_RING; reg_dma2.LISR |= DMA_LISR_TCIF0; }
  if(irq && reg_dma2.LISR) DMA2_Stream0_IRQHandler();
 }
}
static void check_pcm(u32 n) {
 u32 i; assert(Mic_Bytes()==n*2u); assert(!Mic_Error());
 for(i=0;i<n;i++) assert((int16_t)pcm[i]==((int)sample(i)-2048)*16);
}
static void reset_case(void) {
 if(active) Mic_Stop();
 start_fault=tx_fault=0; state=VOICE_IDLE; press_req=release_req=0;
 sent_count=0; ticks=0; Voice_Init(); C3_AudioSession(1); C3_AudioSession(0);
 xQueueReset(q_tx);
}
static void record_ms(u32 ms) {
 Voice_Press(); Voice_Tick(); assert(state==VOICE_RECORD);
 feed(ms*16u,1); ticks += ms; Voice_Release(); Voice_Tick();
}
static void receive(u8 type,u8 seq,const u8 *p,u16 n) {
 u8 wire[PROTO_MAX_FRAME],bad; u16 len,i,crc; proto_frame_t f;
 len=proto_build(wire,type,seq,p,n); wire[3]=PROTO_SRC_C3;wire[4]=PROTO_SRC_F4;
 crc=proto_crc16(wire+2,len-4);wire[len-2]=(u8)crc;wire[len-1]=(u8)(crc>>8);
 for(i=0;i<len;i++) if(proto_feed(wire[i],&f,&bad)) on_frame(&f);
}
static void ack_latest(void) {
 receive(PT_ACK,sent[sent_count-1].data[6],NULL,0); ticks+=5; Voice_Tick();
}
static u32 le32(const u8 *p) { return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24); }
int main(void) {
 unsigned i, tests=0, wraps=0; u8 errcode=PE_HTTP; u32 off; Sent saved;
 static const u32 lengths[]={0,1,1023,1024,1025,2047,2048,2049,6399,6400,
  47999,48000,49152,65535,65536,65537,159999,160000,160001,161792};
 assert(MIC_MAX_MS==10000u && MIC_MAX_BYTES==320000u);
 C3_Init(460800);
 for(i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++) {
  reset_case(); assert(Mic_Start()); feed(lengths[i],1); Mic_Stop();
  check_pcm(lengths[i]>160000?160000:lengths[i]);
  assert(!reg_tim.CR && !reg_adc.CR && !reg_dma.CR); tests++;
 }
 reset_case(); assert(Mic_Start()); feed(1030,0); Mic_Stop(); check_pcm(1030);tests++;
 reset_case(); assert(Mic_Start()); feed(1024,1); feed(1030,0); Mic_Stop(); check_pcm(2054);tests++;
 reset_case(); assert(Mic_Start()); feed(2048,0); DMA2_Stream0_IRQHandler();
 assert(Mic_Error() & MIC_ERR_LATE); assert(!Mic_Busy()); Mic_Stop();tests++;
 reset_case(); record_ms(399); assert(state==VOICE_IDLE && !sent_count);tests++;
 reset_case(); record_ms(400); assert(state==VOICE_UPLOAD && sent_count==1);tests++;
 reset_case(); record_ms(5123); assert(state==VOICE_UPLOAD); check_pcm(81968);tests++;
 reset_case(); tx_seq=0; record_ms(10000); assert(state==VOICE_UPLOAD); check_pcm(160000);
 assert(le32(sent[0].data+9)==320000);
 while(state==VOICE_UPLOAD) ack_latest();
 assert(state==VOICE_WAIT && sent_count==635);
 for(i=1;i<sent_count;i++) {
  assert(sent[i].data[6]==(u8)(sent[i-1].data[6]+1u));
  if(sent[i].data[6]==0) wraps++;
 }
 assert(wraps==2);
 off=0;
 for(i=1;i<sent_count-1;i++) {
  Sent *f=&sent[i]; u32 n=f->len-PROTO_HEAD_LEN-2-4;
  assert(f->data[5]==PT_AUDIO_DATA && le32(f->data+9)==off);
  assert(n<=506 && !(n&1)); assert(memcmp(f->data+13,Mic_Data()+off,n)==0);off+=n;
 }
 assert(off==320000 && sent[634].data[5]==PT_AUDIO_END);
 assert(sent[633].len-PROTO_HEAD_LEN-2-4==208);tests++;
 Voice_Press(); Voice_Tick();assert(state==VOICE_WAIT);tests++;
 receive(PT_ASR_TEXT,99,(const u8*)"raw reply: open light",21);Voice_Tick();
 assert(state==VOICE_RESULT && strcmp(Voice_Text(),"raw reply: open light")==0);tests++;
 reset_case(); record_ms(500);saved=sent[0];
 receive(PT_ACK,(u8)(saved.data[6]+1),NULL,0);Voice_Tick();
 assert(sent_count==1 && state==VOICE_UPLOAD);tests++;
 for(i=1;i<=3;i++) { ticks+=300; Voice_Tick();assert(sent_count==i+1); assert(sent[i].len==saved.len && !memcmp(sent[i].data,saved.data,saved.len)); }
 ticks+=300;Voice_Tick();assert(state==VOICE_FAIL && sent_count==4);tests++;
 reset_case();record_ms(500);ack_latest();saved=sent[1];
 receive(PT_ACK,sent[0].data[6],NULL,0);Voice_Tick();assert(sent_count==2);
 ticks+=300;Voice_Tick();assert(sent_count==3 && !memcmp(saved.data,sent[2].data,saved.len));
 ack_latest();assert(le32(sent[3].data+9)==506);tests++;
 reset_case();record_ms(500);while(state==VOICE_UPLOAD) ack_latest();
 receive(PT_NACK,173,&errcode,1);Voice_Tick();assert(state==VOICE_FAIL);tests++;
 reset_case();record_ms(500);while(state==VOICE_UPLOAD) ack_latest();
 ticks+=8000;Voice_Tick();assert(state==VOICE_FAIL);tests++;
 reset_case();start_fault=1;Voice_Press();Voice_Tick();
 assert(state==VOICE_FAIL && !sent_count);tests++;
 reset_case();tx_fault=1;record_ms(500);assert(state==VOICE_FAIL && !sent_count);tests++;
 reset_case();Voice_Press();Voice_Tick();feed(48000,1);ticks=3000;Voice_Tick();
 assert(state==VOICE_RECORD && Mic_Busy() && !sent_count);
 feed(111984,1);ticks=9999;Voice_Tick();
 assert(state==VOICE_RECORD && Mic_Busy() && !sent_count);
 feed(16,1);ticks=10000;Voice_Tick();
 assert(state==VOICE_UPLOAD && Mic_Bytes()==320000 && !Mic_Busy());
 check_pcm(160000);tests++;
 reset_case();record_ms(500);C3_RequestWeather("test");C3_Tick();
 assert(sent_count==1);tests++;
 printf("PASS: %u capture/protocol/state-machine cases\n",tests);
 return 0;
}
"""
(OUT / "test.c").write_text(HARNESS, encoding="utf-8")
gcc = shutil.which("gcc")
if not gcc:
    raise SystemExit("gcc not found")
cmd = [gcc, "-std=c99", "-O0", "-g", "-Wall", "-Wextra",
       "-I", str(OUT)]
for folder in ("mic", "voice", "proto", "usart_c3"):
    cmd += ["-I", str(ROOT / "APP" / folder)]
cmd += [str(OUT / "test.c"), "-o", str(OUT / "test.exe")]
subprocess.run(cmd, check=True)
result = subprocess.run([str(OUT / "test.exe")], capture_output=True)
(OUT / "result.log").write_bytes(result.stdout + result.stderr)
print(result.stdout.decode("utf-8", errors="replace").replace("\r", "").splitlines()[-1] if result.stdout else "No test output")
if result.returncode:
    print(result.stderr.decode("utf-8", errors="replace"))
    raise SystemExit(result.returncode)
