# F407 端侧固件

STM32F407ZGT6 @ 168 MHz，普中 PZ6808L-F4 开发板。Keil AC5 + STM32 HAL + FreeRTOS + LVGL 8.3。

硬件方案与引脚分配的权威来源：[`../docs/课设方案-AI手表功能原型.md`](../docs/课设方案-AI手表功能原型.md) 第 3 节。

## 构建

Keil 5 打开 `Template.uvprojx`（Target 1）；或用 eide 的无头编译：

```
unify_builder.exe -p "build/Target 1/builder.params" --no-color
```

- 工具链：armcc v5（AC5）
- LVGL 在 `Middlewares/lvgl`，是 submodule，先 `git submodule update --init`
- `build/` 不入库，首次构建后生成
- **新增 .c 必须三处同步登记**：`.eide/eide.yml`、`build/Target 1/builder.params`、
  `Template.uvprojx`。只改一处会漏编或链接失败
- AC5 陷阱：定义在使用之后的 static 函数会被当成隐式外部符号 → 链接 undefined，
  新加内部辅助函数要前向声明

## 目录

| 路径 | 内容 |
| --- | --- |
| `User/main.c` | 初始化顺序 + FreeRTOS 任务创建 |
| `APP/` | 自研代码：BSP（led/beep/key/iic/rtc/tftlcd…）、驱动（mpu6050/ds18b20/eeprom/mic）、逻辑（pedometer/voice/proto/usart_c3）、UI（`APP/ui` 全部 LVGL 页面） |
| `APP/ui/lv_font_chinese_16_trial.c` | 界面中文字库（16px 4bpp，改文案需重新生成/替换） |
| `Libraries/` | STM32F4 HAL + CMSIS |
| `Middlewares/FreeRTOS` | 内核 V10.6.2（MIT），Keil 移植层 `portable/RVDS/ARM_CM4F` |
| `Middlewares/lvgl` | LVGL 8.3.11 submodule（tag v8.3.11） |
| `tools/` | 主机侧验证脚本 + `gen_ui_font.py` |
| `docs/` | 语音链路上板调试记录 |

## FreeRTOS 任务

LVGL 8.3 没有内部锁，**所有 `lv_*` 调用只在 `task_ui`**，其他任务经队列投递事件。

| 任务 | 优先级/栈 | 职责 |
| --- | --- | --- |
| `task_ui` | 3 / 1024 | 唯一碰 LVGL；消费事件队列，刷新标签 |
| `task_link` | 2 / 512 | C3 心跳、语音状态机推进、秒表/倒计时 |
| `task_sensor` | 1 / 512 | 20 ms 读 MPU6050 → 计步/跌倒 |
| `task_slow` | 1 / 512 | RTC、DS18B20 非阻塞转换、闹钟、EEPROM 落盘 |
| `task_game_*` | 1 | 游戏节拍（进入建、退出删） |

## 内存布局要点

- LVGL 双缓冲（20 行 × 320 × 2B × 2）放**内部 SRAM** `0x2000xxxx`
- 10 s PCM（320 KB）放**外部 SRAM**（FSMC NE3，`0x68000000`，IS62WV51216）
- F407 的 DMA 不能访问 CCM（`0x10000000`）；DMA 缓冲和外设句柄必须留在内部 SRAM
- 显示缓冲也曾尝试搬外部 SRAM，红屏失败已回退，不要再按同样做法重试
