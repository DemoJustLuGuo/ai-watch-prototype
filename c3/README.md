# ESP32-C3 网关

ESP-IDF v5.4.4 / `esp32c3`。通过 UART1 接收 F407 协议帧、连接 Wi-Fi，并通过 HTTPS 请求天气或语音对话。ASR_TEXT 按云端返回的简短中文回复转发给 F407，不做关键词控制。

## 工具与构建

在 ESP-IDF 的 PowerShell 终端中构建，不要在 Git Bash 里运行 `idf.py`。

```powershell
idf.py set-target esp32c3
idf.py build
```

`main/secrets.h` 不进版本库。项目版本在根 `CMakeLists.txt` 的 `PROJECT_VER` 设置。启动日志会打印版本、编译时间、IDF 版本、ELF SHA-256 前 8 字节、运行分区和空闲堆。用下面的命令取出 ELF SHA-256，与启动横幅的 16 位十六进制值核对：

```powershell
(Get-FileHash build\ai-watch-c3.elf -Algorithm SHA256).Hash.Substring(0, 16).ToLower()
```

烧录前先扫描并确认串口适配器，再用 `esptool.py --chip esp32c3 -p COMx chip-id` 只读核对芯片身份。确认是 C3 后才烧录；不要用未核对的 COM 口。监视波特率为 115200，退出按 `Ctrl+]`。

## 语音路径

音频 BEGIN/DATA/END 写入独立 `audio` Flash 分区，最大 320000 字节。空闲时后台预擦除，接收时每 4 KB 缓冲后写入；上传任务从分区分块读取 PCM，边生成 WAV Base64 边写 HTTPS 请求体，不在 RAM 中保留整段 PCM、WAV、Base64 或 JSON。UART 接收由独立高优先级任务处理，不会被网络请求同步阻塞。

对话成功后原样发送 ASR_TEXT。提示词要求模型直接回应语音内容，并将回复控制在四十个汉字以内，适配手表显示。失败日志区分连接、上传、HTTP 状态和 JSON 解析。ACK 只表示对应音频帧被接收处理，不表示云端请求成功。最多接收 10 秒、16 kHz、16 bit、单声道 PCM，即 320000 字节。现有协议未包含压缩格式字段，因此当前不使用 ADPCM。

## 引脚

对 F407 的 UART1：TX=GPIO4，RX=GPIO5，460800 8N1，无流控。C3 的 TX 接 F407 的 PC11，RX 接 F407 的 PC10，共地，3.3V 从开发板 P9 取。USB 转串口控制台保留在 UART0。

跌倒帧 0x30 目前只记一行日志。开机天气自检关闭，避免与语音 HTTPS 请求并发；天气仍可由 F407 命令触发。
