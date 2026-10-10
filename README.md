# AI 手表原型 · 云边端协同

[![CI 状态占位](https://img.shields.io/badge/status-course%20project%20done-brightgreen)]()

> 《通信工程综合设计 III》课程设计 —— 题目 2《基于 STM32 微控制器的智能系统设计》
> 在普中 PZ6808L-F4（STM32F407ZGT6）开发板上实现的多功能 AI 手表原型，非可穿戴成品。

**完整课设报告（30 页 PDF）见 [`report/main.pdf`](report/main.pdf)**，设计决策文档见 [`docs/课设方案-AI手表功能原型.md`](docs/课设方案-AI手表功能原型.md)。

---

## 它能做什么

| 功能 | 说明 | 落在哪颗芯片 |
| --- | --- | --- |
| 🕐 LVGL 表盘 | 3.5" HX8357D 320×480，FSMC 16 位并口，本地刷新 | F407 |
| 🚶 计步 + 跌倒粗判 | MPU6050 自写阈值算法（不用 DMP），EEPROM 掉电保存 | F407 |
| 🌡 表温 | DS18B20 非阻塞转换 | F407 |
| 🎙 语音对话 | 按住说话 → 10 s 录音 → 云端大模型回答显示在屏上 | F407 采集 → C3 上云 |
| 🌤 天气查询 | Open-Meteo，五城市切换，一行中文摘要 | C3 → F407 显示 |
| ⏰ 闹钟/秒表/倒计时/计算器/小游戏 | 贪吃蛇、乒乓、俄罗斯方块 | F407 |
| 📴 断网降级 | 断网后时钟、计步、表温照走 —— 架构演示点 | F407 |

## 云边端三层架构

```
 云端   火山方舟语音大模型 · Open-Meteo 天气     （模型推理在云）
  ▲ HTTPS（TLS，仅 C3 直连）
 边缘   ESP32-C3（ESP-IDF v5.4.4）              （联网网关，不碰板级 IO）
  ▲ UART4 460800 8N1 自定义帧协议（CRC16 停等）
 端侧   STM32F407ZGT6 @168 MHz                  （采集、算法、LVGL 界面）
        FreeRTOS · LVGL 8.3 · FSMC · I2C · 1-Wire · ADC+DMA
```

**设计原则**：F407 只做采集、算法与显示；C3 只做联网与协议转换；重识别在云端。
关键词解析、指令执行全部在 F407 本地完成，断网时基础功能不受影响。

## 目录结构

```
├── f407/                      # STM32F407 端侧固件（Keil AC5 + eide）
│   ├── APP/                   #   BSP + 逻辑层 + LVGL 界面（自研代码）
│   ├── User/                  #   main.c：FreeRTOS 任务划分
│   ├── Libraries/             #   STM32F4 HAL
│   ├── Middlewares/           #   FreeRTOS 内核 + LVGL (submodule)
│   └── Template.uvprojx       #   Keil 工程
├── c3/                        # ESP32-C3 边缘网关（ESP-IDF）
│   └── main/                  #   link/wifi/asr/weather 四模块
├── report/                    # 课设报告（LaTeX 源码 + main.pdf）
├── docs/                      # 设计方案 + 调试记录
└── README.md
```

## 构建

### F407（二选一）

- **Keil 5** 打开 `f407/Template.uvprojx`，Target 1，编译
- **eide 无头编译**：`unify_builder.exe -p "build/Target 1/builder.params" --no-color`
  （`build/` 不入库，首次用 Keil/eide 编译后生成）

需要 **armcc v5** 工具链（AC5）。LVGL 为 submodule：

```bash
git clone --recurse-submodules https://github.com/<you>/ai-watch-prototype.git
```

### ESP32-C3

ESP-IDF v5.4.4，PowerShell：

```powershell
idf.py set-target esp32c3
idf.py build
```

### 前置配置（必读）

1. **LVGL submodule**：`git submodule update --init`
2. **中文字库**：改 UI 文案后运行 `python f407/tools/gen_ui_font.py`
   （需要 Pillow + simhei.ttf，生成 `lv_font_chinese_16_trial.c`，放 LVGL 源码树）
3. **密钥**：`cp c3/main/secrets.h.example c3/main/secrets.h`，填入
   Wi-Fi SSID/密码、火山方舟 API Key（`secrets.h` 已被 .gitignore 挡住，不会入库）

## 硬件接线（杜邦线）

| F407 | ESP32-C3 | 说明 |
| --- | --- | --- |
| PC10 (UART4_TX) | GPIO5 (RX) | 460800 8N1 |
| PC11 (UART4_RX) | GPIO4 (TX) | ↑ |
| P9 3.3V / GND | 3V3 / GND | C3 供电 |

麦克风模块 AO→PA5（J01/J4 跳线帽必须拔掉）。详见 `docs/课设方案-AI手表功能原型.md` 第 3 节。

## 技术要点

- **F407↔C3 帧协议**：`A5 5A | VER | SRC | DST | TYPE | SEQ | LEN | PAYLOAD | CRC16`，
  停等重传（300 ms × 3），类型见 `f407/APP/proto/proto.h`
- **10 s 语音不在 C3 RAM 里**：320 KB PCM 落 C3 的 flash `audio` 分区，上传时按块读、
  按块 Base64、按块写 HTTPS 请求体，峰值内存只有几 KB —— F407 侧 PCM 则放在外部
  SRAM（FSMC NE3），内部 128 KB SRAM 留给 LVGL 双缓冲和 FreeRTOS
- **LVGL 无锁**：所有 `lv_*` 调用只在 `task_ui`，其他任务经 FreeRTOS 队列投递事件
- **计步**：合加速度 − 一阶滤波重力基线，动态量过阈回落 + 步间隔门限，阈值可在
  校准页调并落 EEPROM

## 相关文档

- [`report/main.pdf`](report/main.pdf) —— 课设报告（30 页）
- [`docs/课设方案-AI手表功能原型.md`](docs/课设方案-AI手表功能原型.md) —— 完整设计决策
- [`docs/voice_bringup_20260923.md`](docs/voice_bringup_20260923.md) / [`docs/voice_10s_20260923.md`](docs/voice_10s_20260923.md) —— 语音链路上板调试记录
- [`c3/README.md`](c3/README.md) / [`f407/README.md`](f407/) —— 两颗固件各自的说明

## License

课程设计作品，仅供学习参考。LVGL 遵循其原版 MIT License（见 submodule）。
