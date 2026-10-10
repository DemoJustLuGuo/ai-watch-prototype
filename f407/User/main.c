#include "system.h"
#include "usart.h"
#include "SysTick.h"
#include "tftlcd.h"
#include "key.h"
#include "led.h"
#include "beep.h"
#include "ws2812.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui.h"
#include "ui_games.h"
#include "ui_weather.h"
#include "ui_timer.h"
#include "ui_settime.h"
#include "mpu6050.h"
#include "ds18b20.h"
#include "rtc.h"
#include "pedometer.h"
#include "eeprom.h"
#include "usart_c3.h"
#include "voice.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 传感器和慢任务不碰 LVGL，只把结果投进这个队列，由界面任务统一改标签。
   LVGL 8.3 没有内部锁，所有 lv_* 调用必须留在 task_ui。
   软件 I2C 的位延时同样不能被打断，所以 EEPROM 只在 task_slow 里写。 */
typedef struct {
	uint8_t kind;		/* 1 步数  2 时间  3 表温  4 在线  5 跌倒  6 动态量  7 保存 */
	uint32_t a;
	int16_t b;
} ui_evt_t;

static QueueHandle_t q_ui;
static uint8_t mpu_ok;

/* 计步的掉电镜像。今日步数和日期由传感器任务维护，慢任务负责落盘。 */
static eeprom_data_t store;
static uint32_t today_steps;
static uint8_t today_date;
static volatile uint8_t save_req;	/* 1 改阈值  2 清零，慢任务消费 */

static void post(ui_evt_t *e)
{
	xQueueSend(q_ui, e, 0);
}

static void on_save_request(void)
{
	ui_evt_t e;
	e.kind = 7; e.a = 0; e.b = 0;
	post(&e);
}

static void on_time_set(uint8_t h, uint8_t m, uint8_t s)
{
	RTC_Set(h, m, s);
}

static void task_ui(void *arg)
{
	ui_evt_t e;
	char line[192];
	(void)arg;

	for(;;) {
		while(xQueueReceive(q_ui, &e, 0)==pdTRUE) {
			switch(e.kind) {
			case 1: ui_set_steps(e.a); break;
			case 2: ui_set_time((uint8_t)(e.a>>16), (uint8_t)(e.a>>8), (uint8_t)e.a); break;
			case 3: ui_set_temp(e.b, (uint8_t)e.a); break;
			case 4: ui_set_online((uint8_t)e.a); break;
			case 5: ui_show_alert(0); break;
			case 6: ui_set_dyn((uint16_t)e.a); break;
			case 7: save_req = ui_take_save(); break;
			case 8: ui_set_today(e.a); break;
			case 9: ui_timer_set(e.a); break;
			}
		}
		/* 连按期间 C3 会陆续回几行旧城市的结果，只显示最后一行，
		   也就是当前选中城市的。 */
		{
			u8 got=0;
			while(C3_TakeWeather(line, sizeof(line))) got=1;
			if(got) ui_set_weather(line);
		}
		ui_set_voice(Voice_Text());	/* 语音页状态和云端回答 */
		lv_timer_handler();
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

static void task_sensor(void *arg)
{
	uint32_t last = 0;
	uint32_t saved_total = 0;
	uint32_t dyn_acc = 0;		/* 动态量累加，满 100 ms 报一次 */
	(void)arg;

	for(;;) {
		short ax, ay, az;
		uint32_t steps;
		uint16_t dyn = 0;
		uint8_t fall = 0;
		ui_evt_t e;

		vTaskDelay(pdMS_TO_TICKS(20));
		if(!mpu_ok) continue;
		if(MPU6050_Get_Accelerometer(&ax, &ay, &az) != 0) continue;
		steps = Pedometer_Feed(ax, ay, az, &dyn, &fall);

		dyn_acc += 20;
		if(dyn_acc >= 100)
		{
			dyn_acc = 0;
			e.kind = 6; e.a = dyn; e.b = 0;
			post(&e);
		}

		if(fall)
		{
			e.kind = 5; e.a = 0; e.b = 0;
			post(&e);
			BEEP_On();
		}

		if(steps == last) continue;
		today_steps += steps - last;
		last = steps;
		e.kind = 1; e.a = steps; e.b = 0;
		post(&e);
		{
			ui_evt_t t;
			t.kind = 8; t.a = today_steps; t.b = 0;
			post(&t);
		}
		/* 每 10 步记一次，慢任务看见差额才写 EEPROM。 */
		if(steps - saved_total >= 10)
		{
			saved_total = steps;
			if(save_req == 0) save_req = 3;
		}
	}
}

static void task_link(void *arg)
{
	uint32_t sw_base = 0;		/* 秒表起算时刻 */
	uint32_t sw_acc = 0;		/* 秒表已累计 */
	uint32_t cd_left = 0;		/* 倒计时剩余毫秒 */
	uint32_t last_tick = 0;
	uint8_t was_running = 0;
	uint32_t diag_at = 0;
	(void)arg;
	for(;;) {
		uint32_t now = xTaskGetTickCount();
		uint32_t step = (now - last_tick) * portTICK_PERIOD_MS;
		ui_evt_t e;

		LED1 = (now / pdMS_TO_TICKS(1000)) & 1 ? 1 : 0;	/* 每秒翻转一次，链路任务的心跳 */
		C3_Tick();
		Voice_Tick();
		/* 录音只看上键按住没有，不走按键的短按/长按事件。 */
		if(ui_on_voice() && KEY_UpHeld()) Voice_Press();
		else if(Voice_State()==VOICE_RECORD) Voice_Release();

		if((now - last_tick) >= pdMS_TO_TICKS(100)) {
		if(ui_timer_running())
		{
			if(!was_running)
			{
				/* 刚开始：秒表从已累计处续走，倒计时从页面显示的剩余起算。 */
				sw_base = now;
				was_running = 1;
			}
			if(ui_timer_mode() == TIMER_MODE_STOPWATCH)
			{
				e.kind = 9; e.a = sw_acc + (now - sw_base) * portTICK_PERIOD_MS; e.b = 0;
				post(&e);
			}
			else
			{
				if(cd_left == 0) cd_left = 60000;	/* 页面设定由第一次推送覆盖 */
				if(step < cd_left) cd_left -= step;
				else cd_left = 0;
				e.kind = 9; e.a = cd_left; e.b = 0;
				post(&e);
				if(cd_left == 0)
				{
					e.kind = 5; e.a = 1; e.b = 0;	/* 倒计时到点 */
					post(&e);
					BEEP_On();
					was_running = 0;
				}
			}
		}
		else if(was_running)
		{
			if(ui_timer_mode() == TIMER_MODE_STOPWATCH)
				sw_acc += (now - sw_base) * portTICK_PERIOD_MS;
			was_running = 0;
		}
		last_tick = now;
		}

        if((HAL_GetTick() - diag_at) >= 5000u && Voice_State() != VOICE_RECORD && Voice_State() != VOICE_UPLOAD)
        {
            diag_at = HAL_GetTick();
            printf("[watch] link alive stack_free=%lu words\r\n", (unsigned long)uxTaskGetStackHighWaterMark(NULL));
            C3_Report();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
	}
}

static void task_slow(void *arg)
{
	uint32_t beep_off_at = 0;
	uint8_t sub = 0;
	uint8_t alarm_fired = 0;
	(void)arg;
	for(;;) {
		rtc_time_t t;
		int16_t deci;
		ui_evt_t e;

		vTaskDelay(pdMS_TO_TICKS(200));

		/* 蜂鸣响 1 秒后关掉。 */
		if(beep_off_at && xTaskGetTickCount() >= beep_off_at)
		{
			BEEP_Off();
			beep_off_at = 0;
		}
		if(BEEP_IsOn() && beep_off_at == 0)
			beep_off_at = xTaskGetTickCount() + pdMS_TO_TICKS(1000);

		RTC_Get(&t);
		if(today_date && t.day && t.day != today_date)
		{
			today_steps = 0;		/* 跨日，今日步数归零，总步数不动 */
			e.kind = 8; e.a = 0; e.b = 0;
			post(&e);
			if(save_req == 0) save_req = 3;
		}
		today_date = t.day;

		/* 闹钟：开着且时分吻合，这一分钟只响一次。 */
		if(ui_alarm_enabled() && t.hour == ui_alarm_hour() && t.min == ui_alarm_min())
		{
			if(!alarm_fired)
			{
				e.kind = 5; e.a = 2; e.b = 0;
				post(&e);
				BEEP_On();
				alarm_fired = 1;
			}
		}
		else
		{
			alarm_fired = 0;
		}

		if(save_req)
		{
			uint16_t th, gap;
			ui_calib_params(&th, &gap);
			if(save_req == 2)
			{
				Pedometer_SetSteps(0);
				today_steps = 0;
			}
			else
			{
				Pedometer_SetThreshold(th, gap);
			}
			store.total = Pedometer_Steps();
			store.today = today_steps;
			store.date = today_date;
			store.th_mg = Pedometer_Threshold();
			store.gap_ms = Pedometer_Gap();
			store.alarm_on = ui_alarm_enabled();
			store.alarm_h = ui_alarm_hour();
			store.alarm_m = ui_alarm_min();
			EEPROM_Save(&store);
			save_req = 0;
		}

		/* 下面的每秒做一次。 */
		if(++sub < 5) continue;
		sub = 0;

		e.kind=2; e.a=((uint32_t)t.hour<<16)|((uint32_t)t.min<<8)|t.sec; e.b=0;
		post(&e);

		e.kind=3;
		if(DS18B20_Read(&deci) == 0) { e.a=1; e.b=deci; }
		else { e.a=0; e.b=0; }
		post(&e);

		e.kind=4; e.a=C3_IsOnline(); e.b=0;
		post(&e);
	}
}

/* 游戏各自一个任务，进入时建、退出时删。任务只等节拍，
   画面仍然回到 task_ui 里画，LVGL 没有锁。 */
static TaskHandle_t game_task;
static TaskHandle_t game_worker;
static uint32_t game_period;

static void task_game_tick(void *arg)
{
	TickType_t wake = xTaskGetTickCount();
	(void)arg;
	for(;;)
	{
		vTaskDelayUntil(&wake, pdMS_TO_TICKS(game_period ? game_period : 1000u));
		if(game_worker) xTaskNotifyGive(game_worker);
	}
}

static void task_game(void *arg)
{
	uint32_t period = (uint32_t)arg;
	void (*step)(void) = period == 40u ? ui_pong_step
	                  : period == 400u ? ui_snake_step
	                  : ui_tetris_step;
	for(;;)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		step();
	}
}

void ui_game_start(uint8_t which)
{
	uint32_t period = which == 1u ? 40u : which == 2u ? 700u : 400u;
	game_period = period;
	if(game_task) vTaskDelete(game_task);
	game_task = 0;
	game_worker = 0;
	xTaskCreate(task_game, "game", 256, (void *)period, 1, &game_task);
	game_worker = game_task;
}

void ui_game_stop(void)
{
	if(!game_task) return;
	game_worker = 0;
	vTaskDelete(game_task);
	game_task = 0;
}

static void start_tasks(void)
{
	q_ui = xQueueCreate(16, sizeof(ui_evt_t));
	xTaskCreate(task_ui,     "ui",     1024, 0, 3, 0);	/* 唯一碰 LVGL，给足栈 */
	xTaskCreate(task_link,   "link",   512,  0, 2, 0);
	xTaskCreate(task_game_tick, "gtick", 128, 0, 1, 0);
	xTaskCreate(task_sensor, "sensor", 512,  0, 1, 0);
	xTaskCreate(task_slow,   "slow",   512,  0, 1, 0);
	xTaskCreate(RGB_Scroll_Task, "rgb", 256, 0, 1, 0);	/* 5x5 点阵滚 CQUT */
	vTaskStartScheduler();
}

void vApplicationMallocFailedHook(void)
{
	taskDISABLE_INTERRUPTS();
	for(;;);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
	(void)task; (void)name;
	taskDISABLE_INTERRUPTS();
	for(;;);
}

int main(void)
{
	HAL_Init();
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);	//4 位全给抢占优先级，FreeRTOS 要求
	SystemClock_Init(8, 336, 2, 7);	/* HSE 8 MHz -> SYSCLK 168 MHz */
	SysTick_Init(168);
    USART1_Init(115200);
    printf("\r\n[watch] F407 voice bringup %s %s; UART4=460800\r\n", __DATE__, __TIME__);

	/* 先把灯点起来。后面任何一步卡死，灯的状态都能说明走到哪。 */
	LED_Init();
	LED1 = 0;
	BEEP_Init();

	/* 屏幕 + LVGL。初始化里的 delay_ms 发生在调度器启动前，仍走 HAL_Delay */
	TFTLCD_Init();
	SRAM_FSMC_Init();		/* PCM 缓冲放外部 SRAM */
	KEY_Init();
	RGB_LED_Init();		/* PE5 上的 5x5 点阵 */
	lv_init();
	lv_port_disp_init();
	lv_port_indev_init();
	LED1 = 1;		/* 建页面前灯灭。若建完后灯又亮，就是 LVGL 断言。 */
	ui_init();
	LED1 = 0;		/* 界面建完。灯常亮说明卡在这之前。 */
	ui_weather_set_req_cb(C3_RequestWeather);	/* 进天气页只入队，链路任务发送 */
	ui_set_save_cb(on_save_request);
	ui_set_time_cb(on_time_set);

	/* 传感器与时钟。I2C 由 MPU6050_Init 初始化，EEPROM 共用同一条总线。 */
	mpu_ok = (MPU6050_Init() == 0);
	Pedometer_Init();
	EEPROM_Load(&store);
	if(store.valid)
	{
		Pedometer_SetSteps(store.total);
		/* 校准页可以把阈值存高，开机一律回到出厂档，并写回 EEPROM。 */
		if(store.th_mg != EE_TH_DEFAULT || store.gap_ms != EE_GAP_DEFAULT)
		{
			store.th_mg = EE_TH_DEFAULT;
			store.gap_ms = EE_GAP_DEFAULT;
			EEPROM_Save(&store);
		}
		Pedometer_SetThreshold(store.th_mg, store.gap_ms);
		today_steps = store.today;
		today_date = store.date;
		ui_set_calib(store.th_mg, store.gap_ms);
		ui_alarm_set(store.alarm_on, store.alarm_h, store.alarm_m);
		ui_set_steps(store.total);
		ui_set_today(store.today);
	}
	RTC_Init();
	DS18B20_Init();			/* 内部已启动第一次转换 */

	/* 边缘网关链路 */
	C3_Init(460800);
	Voice_Init();		/* 麦克风 PA5，跳线帽要先拔 */
    printf("[watch] voice ready; hold UP on voice page\r\n");

	start_tasks();
	LED1 = 0;			/* 调度器返回说明堆不够 */
	for(;;);
}
