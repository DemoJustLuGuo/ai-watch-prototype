# F407 语音联调记录（2026-09-23）

> 版本说明：本记录的“最终固件”和两次上板结果均指 **3 秒时序修正版**。之后用户将目标调整为 10 秒，当前 `build/Target 1/Template.hex` 与 `build/voice_validation.json` 已更新为尚未烧录的 10 秒版，见 [10 秒录音调整记录](voice_10s_20260923.md)。本记录对应的旧 HEX、AXF、MAP、内存及烧录证明已完整备份到 `build/voice_3s_verified_20260923/`，文件保持原相对路径。`build/voice_flash_verify.json` 仍是 3 秒版的历史烧录证明，不能用于证明当前 10 秒版已烧录。

本轮只修改 F407。C3 源码、配置及密钥文件的 SHA-256 与开始时一致。

## 最终状态

最终时序修正版已烧录，OpenOCD 返回 Verified OK。两次实板 3 秒录音均完成采集、PCM 写入读回检查与逐帧 ACK 上传，未出现采集、存储或链路错误；两次均在等待 ASR 文字阶段超时。两次测试只能证明本轮记录通过，不能代替长期稳定性或语音可辨识度验收。

当前应用已通过调试器恢复运行，最后日志中 C3 为 online=1。本轮不再改固件或追加复位。完整语音识别仍受 C3 运行镜像来源不一致的问题阻碍；普通复位及断电上电启动也尚未通过验收，详见下文。

## 已修改

- `APP/mic`：16 kHz TIM3 触发 ADC1；2048 点循环 DMA 分成两个 1024 点半块，在半满/全满中断转存 PCM；松手根据冻结后的 NDTR 补齐尾部。停止时关闭 TIM3/ADC/DMA，检查延迟覆盖、ADC/DMA 错误。PCM 上限 48000 点 / 96000 字节。
- `APP/tftlcd/tftlcd.c`：仅修改 `SRAM_FSMC_Init`：补齐 PG10/NE3 与 PE0/PE1（NBL0/NBL1）的 AF12 配置；连续录音发现偶发写入读回错误后，将 bank 3 地址建立改为 3、数据建立改为 12、周转改为 2，数据阶段约 77 ns。该函数之外内容与原文件逐字节一致。
- PCM 每个半字写入后读回核对，失败标志 `MIC_ERR_MEMORY=0x20`，禁止上传。`[mic] adc_min/max/mean/clip` 从原始 ADC 转存时统计。
- `APP/voice`、`APP/usart_c3`：按下/松开事件由 link 任务处理；ACK 核对 SEQ，300 ms 超时原帧同序号重发，最多 3 次；处理识别期间 NACK；等待回答期间不接受新录音，音频会话暂缓天气请求。现有 C3 的 NACK 使用独立序号，F407 保持兼容，日志显示错误码。
- link 任务每 5 ms 推进语音，计时器业务仍为 100 ms；任务栈和优先级不变。增加空闲栈水位日志。
- USART1 115200 输出阶段日志；中断优先级仍为 6，移除接收 ISR 中的无限等待。2 KB 内部 RAM 环形区镜像 printf，支持 SWD 读取。

## 第一轮实板记录

文件：`build/voice_first_record_swd.log`。

- 3 秒，48000 点，96000 字节；BEGIN / 190 个 DATA / END 获 C3 确认。
- 上行耗时 3097 ms，crc=0 / ore=0 / drop=0 / stale_ack=0。
- link 栈最低剩余 377 words。
- 页面识别超时；COM5 同期读到 C3 的 `wifi: 断开，5 秒后重连`。
- 此轮音频内容无效：SWD 读取原始 DMA 数据为 512～533，而 PCM 区全为 0xFFFF。GPIOG10、GPIOE0/1 实测仍为输入模式，源码也漏配这三脚。第一轮旧 `[mic] adc_min=2048` 是从无效 PCM 反算的，不能当作真实 ADC 值。

## 修正后的验证

- `python tools/test_voice_host.py`：30 组通过，直接编译实际 mic/voice/C3/proto C 源码，替换 HAL/RTOS 为主机模拟。
- `python tools/verify_voice_layout.py`：通过；PCM=0x68000000，raw=0x200000E8，三个句柄在内部 SRAM；RW_IRAM1=0x0001A158；启动表两条目的目的地址均为内部 SRAM。
- AC5 / EIDE Target 1 编译成功。此次重编译 tftlcd.c 显示 LCD_ReadPoint 原有的 6 个未使用变量警告；无编译错误。
- HEX：`build/Target 1/Template.hex`，最终版本 SHA-256 `32781c57a67704a0c06cd18c4202253c013e482b033d8cbf89f6a3c66222025e`。
- pyOCD 两次在编程中失去响应，探针需重新插拔。改用现有 OpenOCD 0.12.0、CMSIS-DAP UID 0602A0002016、SWD 500 kHz 后完成烧录并返回 Verified OK。
- 另外完整读回 Flash，363980 字节与 HEX 逐字节一致。该结果属于引脚修正版，证据为 `build/voice_flash_verify_pcm_pins.json`；之后的时序修正版 OpenOCD 同样 Verified OK，但追加全量读回时探针 HID 断线，完整读回未完成，见当前 `build/voice_flash_verify.json`。
- OpenOCD 复位后实际进入系统 Bootloader：地址 0 的向量为 MSP=0x20001D80、Reset=0x1FFF3DA1，与系统 ROM 相同。GPIOE/G 未初始化，printf 镜像也未初始化。因此当时心跳不闪不是应用已经运行后死锁。
- 引脚修正版曾经探针复位暂停、将向量映射及 VTOR 指回 Flash，并设置该版 MSP=0x2001A150、PC=0x08000261 后恢复运行，读到启动及心跳。证据：`build/voice_flash_entry_boot.log`。最终时序版的 Flash 初始 MSP 已变为 0x2001A158，恢复运行须以当前固件向量为准。正常复位/上电问题的后续验证见下文。

## SRAM 引脚修正后的实板录音

证据：`build/voice_after_pcm_fix_swd.log`、`build/voice_last_record.json`、`build/voice_last_record.wav`。这里的 WAV/PCM/JSON 是引脚修正后的 1.609 秒历史录音，未被最后两次 3 秒录音覆盖。

- 用户按住后松手：25757 点，51514 字节，1609.8125 ms；ADC 406～528，平均 460，削顶点 0，采集及 PCM 写入读回错误 0。
- BEGIN、DATA、END 均获确认，上传 1675 ms；crc=0、ore=0、drop=0、stale_ack=0，link 栈剩余 381 words。
- 完整导出此段 PCM，全部样本满足 12 位 ADC 转 16 位 PCM 的低 4 位为零。PCM 最小 -26592，最大 -24672，平均 -25405.61，去均值后的 RMS=36.98。当前仍按固定中点 2048 转换，录音偏置明显、交流幅度较小；尚未做软件去直流或增益处理，也未完成云端听辨验证。
- 该轮页面仍识别超时，当时 C3 在线状态为 0；之后用户已恢复网络，联网后的结果见下文。

## 联网后的追加发现

- C3 恢复 online=1 后仍识别超时。COM5 实际打印 `link: 音频帧 0x10/0x11/0x12，…，先回 ACK`。
- 这条字符串既不在当前 `c3/main/link.c` 中，也不在当前磁盘 `c3/build/ai-watch-c3.bin` 中（该文件 1038816 字节）。源码调用 asr_begin/asr_data/asr_end，因此板上运行镜像与当前源码/产物不一致。ACK 只能确认帧被应答，不能证明音频已送云。
- 实测摘录：`build/c3_runtime_observation.txt`。本轮没有修改、烧录或复位 C3。
- 后续连续录音有两次被 PCM 读回检查拦截（样本 1028、29697，err=0x20），据此增加 bank 3 时序余量，并补充故障样本序号、预期半字、实际半字日志。该最终版本已编译及 OpenOCD 校验通过，重连探针后的两次实板复测结果如下。

## 最终时序版：两次连续实板录音

用户确认两次录音操作已完成。完整日志：`build/voice_final_two_records_swd.log`；最终启动日志：`build/voice_final_boot_swd.log`。

| 项目 | 第一轮 | 第二轮 |
| --- | --- | --- |
| 采样点 / PCM 字节 | 48000 / 96000 | 48000 / 96000 |
| 录音时长 | 3000 ms | 3000 ms |
| ADC 最小 / 最大 / 平均 | 50 / 202 / 155 | 133 / 234 / 156 |
| 削顶点 | 0 | 0 |
| 采集及 PCM 检查错误 | err=0x00 | err=0x00 |
| BEGIN / DATA / END | 全部获得 ACK | 全部获得 ACK |
| 上传耗时 | 3098 ms | 3101 ms |
| ASR 结果 | 8000 ms 超时 | 8000 ms 超时 |

两轮日志均为 online=1、crc=0、ore=0、drop=0、stale_ack=0。两轮之后 link 栈最低剩余 375 words。最终 HEX 文件 SHA-256 已再次核对，与 `build/voice_validation.json` 和 `build/voice_flash_verify.json` 一致。最终镜像有 OpenOCD 校验成功证据，但没有完成额外全量 Flash 转储；`build/voice_flash_readback.bin` 是未完成的转储，不能当作最终镜像的完整比对证明。

最终两轮 ADC 平均值仍远低于 PCM 换算假定的中点 2048。结合前一段导出录音的明显直流偏置和较小交流幅度，目前只能确认采样与传输流程，尚不能确认音频能被正确听辨。最后两轮没有额外导出 WAV，不把历史 WAV 当作最终录音证据。后续应核对麦克风 AO 的静态电压、说话时摆幅及录音内容，再决定是否需要偏置或增益处理。

## BOOT 跳线与启动验收

BOOT 位置已从《2--天马F407开发板入门教程.pdf》第 4～5 页核实：主芯片右下、四个黄色按键左上方的 2×3 排针，BT0/BT1 分别短接下方 GND。用户已确认 BOOT0 与 GND 短接，并发现 BOOT1 的 GND 跳线帽脱落后将其补回。

补回 BOOT1 后的一次普通调试器复位，仍读到：

```text
0x00000000: 20001D80 1FFF3DA1
0x08000000: 2001A158 08000261
PC:         1FFF3DA0
```

这说明该次复位实际进入系统 ROM，并没有从 Flash 应用启动。BOOT0 为低时，BOOT1 通常不影响 Flash 启动选择，因此不能将补回 BOOT1 认定为故障已修复。跳线位置确认与复位采样时 BOOT0 的实际电平是两项不同证据；USB2 的 CH340 自动下载电路也连接 BOOT0，后续需要核对实际电平及这条电路。

随后通过调试器复位暂停、恢复 Flash 映射和 VTOR、按最终 Flash 向量设置 MSP/PC 后恢复应用。最终启动日志与上述两次录音日志证明应用恢复运行。普通复位与断电重上电仍未通过验收；板子当前保持运行，不需为整理记录再次复位。

## 后续联调边界

下一项识别回程工作是核对 C3 板上实际固件版本。运行日志表现疑似早期仅回复 ACK 的实现，但未读取板上完整镜像，不能据此断言其全部行为。当前磁盘源码和产物均不含该日志字符串；需先解决运行版本与磁盘版本不一致，再验证音频保存、云端识别及 ASR_TEXT 回传。本轮没有修改、烧录或复位 C3，也没有通过单纯延长 F407 超时来掩盖问题。

## 使用的工具

```powershell
python tools/test_voice_host.py
python tools/verify_voice_layout.py
python tools/verify_f407_flash.py
python tools/capture_voice_debug.py --duration 50 --output build/voice_swd.log
python tools/capture_voice_pcm.py
```

`verify_f407_flash.py` 对 HEX 覆盖的全部 Flash 字节进行读回比较；`capture_voice_debug.py` 只以 attach 模式读取日志，不复位、不暂停 CPU。AXF 必须与烧录固件匹配。

当前系统枚举：COM5 是 ESP32 CH343；CH340 被分配 COM4，但 Code 31，且蓝牙串口同名。不要将 COM3 当作已确认的 F407 串口。探针 UID 为 0602A0002016，目标读到 Cortex-M4、DEV_ID 0x413、Flash 1024 KB。

原始 F407 文件和固件备份：`build/voice_baseline_20260923/`；差异：`build/voice_changes.patch`。

录音导出工具会保存 `build/voice_last_record.wav`（原始 16 kHz/16 bit/单声道，无去直流或增益处理）、PCM、最后 DMA 环形块及统计 JSON。
