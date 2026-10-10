# AI 手表原型

普中 PZ6808L-F4 上的课设工程。方案说明在上一级的 `课设方案-AI手表功能原型.md`。

## 已经定下来的

- 屏：3.5 寸 **HX8357D**，320×480，FSMC 16 位。资料里有同名原理图和数据手册。`tftlcd.h` 已改成 `TFTLCD_HX8357D`，竖屏。
- 地址宏沿用普中实验：`0x6C000000 | 0x0000007E`。
- F407 用 Keil + 这套 HAL。工程从「基础实验 HAL 版」的实验 33（FSMC-TFTLCD）拷出。
- 蜂鸣器在 PZ6808L 上是低电平响。例程原板 T200/T300 是高电平，不要照抄例程里的蜂鸣极性。
- 界面后续用 LVGL 8.3，局部刷新。本目录现在只到「屏能点亮」。
- C3、语音密钥、城市经纬度、计步阈值上板后再填，不挡当前编译。

## 目录

| 路径 | 内容 |
| --- | --- |
| `f407/Template.uvprojx` | Keil 工程，芯片 STM32F407ZG |
| `f407/APP/tftlcd` | HX8357D 驱动 |
| `f407/APP/iic` `mpu6050` `ds18b20` `rtc` `beep` | 传感器/外设 BSP（普中例程风格，引脚按方案 3.2） |
| `f407/APP/pedometer` | 计步（方案 4.2，自写阈值，不用 DMP） |
| `f407/APP/proto` `usart_c3` | F407↔C3 帧协议（方案 5.1）与 UART4 链路 |
| `f407/APP/ui` | LVGL 五页界面 + `ui_font.c`（界面汉字字体） |
| `f407/tools/gen_ui_font.py` | 用 simhei 重新生成 `ui_font.c`；改文案后跑一次 |
| `c3/` | ESP32-C3 网关，尚未建固件 |

## 本步进度（时间/计步/天气逻辑）

- BSP：软件 I2C（PB8/PB9）、MPU6050（0x68，50Hz/±2g）、DS18B20（PG9，非阻塞转换）、RTC（LSE，首次上电用编译时间设初值）、蜂鸣器（PF8 低电平）。
- 逻辑：计步（合加速度-重力基线，阈值 0.15g/300ms）、CRC16-Modbus 组帧与收帧状态机、UART4 460800 心跳/天气/识别回程。
- 界面接线：`main.c` 每拍把 RTC 时间、表温、计步、在线状态、天气文本推给 UI；状态栏「时钟 / 表温 / 在线离线」，计步页实时数字，天气页显示 C3 回传文本（进页请求一次）。
- C3 未接时：时钟、计步、表温照走，状态栏显示「离线」——符合方案的断网演示。
- 语音链路（麦克风采样、分包上传）留到麦克风那步再接。

## 构建

Keil 5 打开 `f407/Template.uvprojx`；或用 eide（VS Code）。命令行无头编译（当前使用）：

```
unify_builder.exe -p "build/Target 1/builder.params" --no-color
```

`unify_builder.exe` 在 `~/.vscode/extensions/cl.eide-*/res/tools/win32/unify_builder/`。
新增 `.c` 要同时登记到 `.eide/eide.yml`、`build/Target 1/builder.params`、`Template.uvprojx` 三处。
当前占用：Code≈210KB，RAM 56.9%（72.9/128KB），ROM 31.4%。

下载切换停在 F4。第一件事是上电看屏是否点亮；花屏再改 FSMC 时序，不要先改分辨率。
