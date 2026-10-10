#include "asr.h"
#include "secrets.h"
#include "link.h"
#include "proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "asr";

/* 16 kHz、16 bit、单声道，最长 10 秒 = 320000 字节。
   C3 只有 400 KB SRAM，联网加 TLS 之后剩不下整段 PCM，所以整段落到 flash 的
   audio 分区，上传时再一边读一边做 Base64 推给 HTTPS，峰值内存只有几个小缓冲。 */
#define ASR_RATE       16000
#define ASR_MAX_MS     10000
#define ASR_MAX_PCM    (ASR_RATE * 2 * (ASR_MAX_MS / 1000))   /* 320000 */

#define ASR_SECTOR     4096                     /* flash 擦除粒度 */
#define ASR_PREERASE   ((ASR_MAX_PCM + ASR_SECTOR - 1) / ASR_SECTOR * ASR_SECTOR)
#define ASR_READ_CHUNK 3072                     /* 3 的倍数，Base64 按组走 */
#define ASR_B64_OUT    3072                     /* 4 的倍数 */
#define ASR_TEXT_MAX   191                      /* F407 的 c3_text_t.text 是 192 字节 */
#define ASR_WAV_HEAD   44

/* 生成适合手表屏幕显示的简短对话回复。ASR_TEXT 仍作为文本载荷回传。
   注意这段会直接进 JSON 字符串，不要出现引号或反斜杠。 */
static const char *PROMPT =
    "请直接用简短自然的中文回应这段语音，像 AI 助手与用户对话。用户提问时直接回答，用户陈述时作合适回应。不要转写语音或解释识别过程，回复控制在四十个汉字以内。听不清时请简短请用户重说。";

typedef enum { ASR_IDLE = 0, ASR_RECV, ASR_UPLOAD } asr_state_t;

static const esp_partition_t *s_part;
static SemaphoreHandle_t      s_lock;           /* 护住状态机和 flash 访问 */

static asr_state_t s_state = ASR_IDLE;
static uint32_t    s_total;                     /* BEGIN 报的总字节数 */
static uint32_t    s_got;                       /* 已落盘的连续字节数 */
static uint32_t    s_pre;                       /* [0, s_pre) 已擦好，可直接写 */
static int64_t     s_t_begin;

static uint8_t  s_buf[ASR_SECTOR];              /* 攒满一个扇区再写 flash */
static uint32_t s_nbuf;

/* ------------------------------------------------------------------ flash */

/* 把 s_buf 里的 s_nbuf 字节写到 off 处。需要时先擦所在扇区。 */
static bool flash_flush(uint32_t off)
{
    if (s_nbuf == 0) return true;

    uint32_t sector = off / ASR_SECTOR * ASR_SECTOR;
    if (off + s_nbuf > s_pre) {                 /* 这个扇区还没预擦 */
        esp_err_t e = esp_partition_erase_range(s_part, sector, ASR_SECTOR);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "擦除 0x%06X 失败：%s", (unsigned)sector, esp_err_to_name(e));
            return false;
        }
        s_pre = sector + ASR_SECTOR;
    }

    esp_err_t e = esp_partition_write(s_part, off, s_buf, s_nbuf);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "写入 0x%06X 失败：%s", (unsigned)off, esp_err_to_name(e));
        return false;
    }
    s_nbuf = 0;
    return true;
}

/* 空闲时一个扇区一个扇区地预擦，把擦除时间挪出录音过程。
   每次只持锁一个扇区（约几十毫秒），不会顶掉 F407 的 300 ms 停等。 */
static void preerase_task(void *arg)
{
    for (;;) {
        bool done = false;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_state == ASR_IDLE && s_pre < ASR_PREERASE) {
            if (esp_partition_erase_range(s_part, s_pre, ASR_SECTOR) == ESP_OK) {
                s_pre += ASR_SECTOR;
            } else {
                done = true;                    /* 擦不动就别刷日志了 */
            }
        } else {
            done = true;
        }
        xSemaphoreGive(s_lock);

        vTaskDelay(done ? pdMS_TO_TICKS(200) : 1);
    }
}

/* ----------------------------------------------------------------- base64 */

/* 流式 Base64：分块喂进来，攒够 3 字节出 4 个字符，直接写进 HTTP 请求体。
   carry 跨块保留 0～2 字节，所以 WAV 头和 PCM 可以接着编码，不会各自补 =。 */
typedef struct {
    esp_http_client_handle_t c;
    uint8_t  carry[3];
    uint8_t  ncarry;
    char     out[ASR_B64_OUT];
    uint16_t nout;
    bool     err;
} b64_t;

static bool http_write_all(esp_http_client_handle_t c, const char *p, int len)
{
    while (len > 0) {
        int n = esp_http_client_write(c, p, len);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

static void b64_drain(b64_t *s)
{
    if (s->nout && !s->err) {
        if (!http_write_all(s->c, s->out, s->nout)) s->err = true;
    }
    s->nout = 0;
}

static void b64_emit(b64_t *s, const uint8_t *g, int n)
{
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    uint32_t v = (uint32_t)g[0] << 16;
    if (n > 1) v |= (uint32_t)g[1] << 8;
    if (n > 2) v |= g[2];

    s->out[s->nout++] = T[(v >> 18) & 63];
    s->out[s->nout++] = T[(v >> 12) & 63];
    s->out[s->nout++] = (n > 1) ? T[(v >> 6) & 63] : '=';
    s->out[s->nout++] = (n > 2) ? T[v & 63] : '=';

    if (s->nout >= ASR_B64_OUT) b64_drain(s);
}

static void b64_push(b64_t *s, const uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    if (s->ncarry) {
        while (s->ncarry < 3 && i < len) s->carry[s->ncarry++] = data[i++];
        if (s->ncarry < 3) return;
        b64_emit(s, s->carry, 3);
        s->ncarry = 0;
    }
    while (len - i >= 3) { b64_emit(s, data + i, 3); i += 3; }
    while (i < len) s->carry[s->ncarry++] = data[i++];
}

static void b64_end(b64_t *s)
{
    if (s->ncarry) { b64_emit(s, s->carry, s->ncarry); s->ncarry = 0; }
    b64_drain(s);
}

/* -------------------------------------------------------------------- wav */

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = v >> 24;
}
static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF; p[1] = v >> 8;
}

/* 44 字节标准 WAV 头。头留在内存里，flash 上只放纯 PCM，
   这样长度对不上时不用回头改写第 0 个扇区。 */
static void build_wav_head(uint8_t *h, uint32_t pcm_len)
{
    memcpy(h, "RIFF", 4);
    put_u32(h + 4, 36 + pcm_len);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_u32(h + 16, 16);
    put_u16(h + 20, 1);                 /* PCM */
    put_u16(h + 22, 1);                 /* 单声道 */
    put_u32(h + 24, ASR_RATE);
    put_u32(h + 28, ASR_RATE * 2);      /* 字节率 */
    put_u16(h + 32, 2);                 /* 块对齐 */
    put_u16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    put_u32(h + 40, pcm_len);
}

/* ------------------------------------------------------------------- 上传 */

/* 按 UTF-8 字符边界截断，别把一个汉字切一半发给 F407。 */
static void utf8_clamp(char *dst, int cap, const char *src)
{
    int n = (int)strlen(src);
    if (n > cap - 1) {
        n = cap - 1;
        while (n > 0 && (src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
}

/* 从应答里挑出第一段 output_text。顺手把 error.message 打到日志。 */
static bool pick_text(const char *body, char *out, int cap)
{
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGE(TAG, "应答不是 JSON");
        return false;
    }

    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (cJSON_IsObject(err)) {
        cJSON *msg  = cJSON_GetObjectItem(err, "message");
        cJSON *code = cJSON_GetObjectItem(err, "code");
        ESP_LOGE(TAG, "方舟错误 code=%s msg=%s",
                 cJSON_IsString(code) ? code->valuestring : "-",
                 cJSON_IsString(msg)  ? msg->valuestring  : "-");
    }

    bool ok = false;
    cJSON *item;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(root, "output")) {
        cJSON *part;
        cJSON_ArrayForEach(part, cJSON_GetObjectItem(item, "content")) {
            cJSON *type = cJSON_GetObjectItem(part, "type");
            cJSON *text = cJSON_GetObjectItem(part, "text");
            if (!cJSON_IsString(text) || text->valuestring[0] == 0) continue;
            if (cJSON_IsString(type) && strcmp(type->valuestring, "output_text") != 0) continue;
            utf8_clamp(out, cap, text->valuestring);
            ok = true;
            break;
        }
        if (ok) break;
    }

    cJSON_Delete(root);
    if (!ok) ESP_LOGE(TAG, "应答里没有 output_text");
    return ok;
}

/* 读完整个应答体。Content-Length 有就按它分配，没有（分块）就按需扩到上限，
   免得截断一个读不动的 JSON。 */
static char *read_body(esp_http_client_handle_t c, int hint)
{
    int cap = (hint > 0) ? hint + 1 : 4096;
    if (cap > 16384) cap = 16384;

    char *buf = malloc(cap);
    if (!buf) return NULL;

    int got = 0;
    for (;;) {
        if (got >= cap - 1) {
            if (cap >= 16384) break;            /* 够判断错误了，不再长 */
            int ncap = cap * 2 > 16384 ? 16384 : cap * 2;
            char *nb = realloc(buf, ncap);
            if (!nb) break;
            buf = nb;
            cap = ncap;
        }
        int n = esp_http_client_read(c, buf + got, cap - 1 - got);
        if (n <= 0) break;
        got += n;
    }
    buf[got] = 0;
    return buf;
}

/* 把 flash 里的 pcm_len 字节当 WAV 发给方舟，回答写进 reply。
   请求体不落地：Content-Length 先算出来，正文一边编码一边写。 */
static bool ark_upload(uint32_t pcm_len, char *reply, int reply_cap)
{
    uint8_t  head[ASR_WAV_HEAD];
    uint32_t wav_len = ASR_WAV_HEAD + pcm_len;
    uint32_t b64_len = (wav_len + 2) / 3 * 4;

    build_wav_head(head, pcm_len);

    char prefix[512];
    int  plen = snprintf(prefix, sizeof(prefix),
        "{\"model\":\"%s\",\"input\":[{\"role\":\"user\",\"content\":["
        "{\"type\":\"input_audio\",\"audio_url\":\"data:audio/wav;base64,",
        ARK_MODEL);

    char suffix[512];
    int  slen = snprintf(suffix, sizeof(suffix),
        "\"},{\"type\":\"input_text\",\"text\":\"%s\"}]}]}", PROMPT);

    if (plen <= 0 || plen >= (int)sizeof(prefix) ||
        slen <= 0 || slen >= (int)sizeof(suffix)) {
        ESP_LOGE(TAG, "请求头尾拼装越界");
        return false;
    }

    esp_http_client_config_t cfg = {
        .url               = "https://ark.cn-beijing.volces.com/api/v3/responses",
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 30000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    ESP_LOGI(TAG, "HTTP 准备：空闲堆 %u，最大块 %u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        ESP_LOGE(TAG, "HTTP 客户端创建失败");
        return false;
    }

    b64_t   *b64 = calloc(1, sizeof(b64_t));
    uint8_t *rd  = malloc(ASR_READ_CHUNK);
    if (!b64 || !rd) {
        ESP_LOGE(TAG, "上传缓冲分配失败：空闲堆 %u，最大块 %u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        free(b64); free(rd);
        esp_http_client_cleanup(c);
        return false;
    }
    b64->c = c;

    /* 头必须在 open 之前设：open 里就把请求行和所有头发出去了。 */
    char auth[160];
    snprintf(auth, sizeof(auth), "Bearer %s", ARK_KEY);
    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", "application/json");

    int  content_len = plen + (int)b64_len + slen;
    bool ok = false;
    int64_t t0 = esp_timer_get_time();

    ESP_LOGI(TAG, "上传 PCM %u 字节，WAV %u，请求体 %d 字节",
             (unsigned)pcm_len, (unsigned)wav_len, content_len);

    esp_err_t e = esp_http_client_open(c, content_len);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "连接失败：%s，耗时 %lld ms", esp_err_to_name(e),
                 (esp_timer_get_time() - t0) / 1000);
        goto out;
    }

    if (!http_write_all(c, prefix, plen)) { ESP_LOGE(TAG, "写请求头失败"); goto out; }

    b64_push(b64, head, ASR_WAV_HEAD);
    for (uint32_t off = 0; off < pcm_len && !b64->err; ) {
        uint32_t n = pcm_len - off;
        if (n > ASR_READ_CHUNK) n = ASR_READ_CHUNK;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        esp_err_t re = esp_partition_read(s_part, off, rd, n);
        xSemaphoreGive(s_lock);
        if (re != ESP_OK) {
            ESP_LOGE(TAG, "读 flash 0x%06X 失败：%s", (unsigned)off, esp_err_to_name(re));
            goto out;
        }
        b64_push(b64, rd, n);
        off += n;
    }
    b64_end(b64);
    if (b64->err) { ESP_LOGE(TAG, "写音频正文失败"); goto out; }

    if (!http_write_all(c, suffix, slen)) { ESP_LOGE(TAG, "写请求尾失败"); goto out; }

    int clen = esp_http_client_fetch_headers(c);
    int code = esp_http_client_get_status_code(c);
    int64_t t_sent = esp_timer_get_time();

    char *body = read_body(c, clen);
    ESP_LOGI(TAG, "HTTP %d，正文 %d 字节，发送 %lld ms，总计 %lld ms",
             code, clen, (t_sent - t0) / 1000,
             (esp_timer_get_time() - t0) / 1000);

    if (!body) {
        ESP_LOGE(TAG, "应答缓冲分配失败");
    } else {
        if (code != 200) ESP_LOGE(TAG, "应答：%.256s", body);
        ok = (code == 200) && pick_text(body, reply, reply_cap);
        free(body);
    }

out:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    free(b64);
    free(rd);
    return ok;
}

static void upload_task(void *arg)
{
    uint32_t pcm_len = s_got;
    char     reply[ASR_TEXT_MAX + 1];

    if (ark_upload(pcm_len, reply, sizeof(reply))) {
        link_send_text(PT_ASR_TEXT, reply);
        ESP_LOGI(TAG, "AI 回复：%s", reply);
    } else {
        link_send_nack(PE_HTTP);
    }

    ESP_LOGI(TAG, "本轮结束，剩余堆 %u，最大块 %u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pre   = 0;                 /* 刚写过的扇区脏了，交给后台重新预擦 */
    s_nbuf  = 0;
    s_state = ASR_IDLE;
    xSemaphoreGive(s_lock);

    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------- 接口 */

void asr_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                      ESP_PARTITION_SUBTYPE_ANY, "audio");
    if (!s_lock || !s_part) {
        ESP_LOGE(TAG, "audio 分区或锁不可用，语音上传不可用");
        return;
    }
    if (s_part->size < ASR_PREERASE) {
        ESP_LOGE(TAG, "audio 分区 %u 字节，装不下 %u",
                 (unsigned)s_part->size, (unsigned)ASR_PREERASE);
    }
    ESP_LOGI(TAG, "暂存分区 0x%06X 大小 %u，上限 %u 字节（%d 秒）",
             (unsigned)s_part->address, (unsigned)s_part->size,
             (unsigned)ASR_MAX_PCM, ASR_MAX_MS / 1000);

    xTaskCreate(preerase_task, "asr_pre", 3072, NULL, 1, NULL);
}

void asr_begin(uint32_t total, uint8_t seq)
{
    if (!s_part) { link_send_nack(PE_HTTP); return; }

    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_state == ASR_UPLOAD) {               /* 上一段还在传，先别收新的 */
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "仍在上传，拒收新的 BEGIN");
        link_send_nack(PE_BUSY);
        return;
    }
    if (total == 0 || total > ASR_MAX_PCM || total > s_part->size) {
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "BEGIN 总长度 %u 超出上限 %u", (unsigned)total, (unsigned)ASR_MAX_PCM);
        link_send_nack(PE_BUSY);
        return;
    }

    /* BEGIN 重发可能发生在某个 DATA 已到达后，不能因此清空已收音频。 */
    bool resend = (s_state == ASR_RECV && s_total == total);
    if (!resend) {
        s_total   = total;
        s_got     = 0;
        s_nbuf    = 0;
        s_state   = ASR_RECV;
        s_t_begin = esp_timer_get_time();
    }
    xSemaphoreGive(s_lock);

    if (!resend) ESP_LOGI(TAG, "开始接收 %u 字节", (unsigned)total);
    link_send_ack(seq);
}

void asr_data(uint32_t offset, const uint8_t *pcm, uint16_t len, uint8_t seq)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_state != ASR_RECV || offset > s_total || len == 0 || len > s_total - offset) {
        xSemaphoreGive(s_lock);
        link_send_nack(PE_BUSY);
        return;
    }
    /* 重发的 DATA：整段都在已落盘范围内，只补 ACK，不重复累计。 */
    if (offset + len <= s_got) {
        xSemaphoreGive(s_lock);
        link_send_ack(seq);
        return;
    }
    /* F407 是停等发送，偏移必须接着上一帧。跳号说明中间丢了。 */
    if (offset != s_got) {
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "偏移不连续：收到 %u，期望 %u", (unsigned)offset, (unsigned)s_got);
        link_send_nack(PE_BUSY);
        return;
    }

    bool bad = false;
    uint16_t i = 0;
    while (i < len && !bad) {
        uint32_t room = ASR_SECTOR - s_nbuf;
        uint32_t n    = len - i;
        if (n > room) n = room;

        memcpy(s_buf + s_nbuf, pcm + i, n);
        s_nbuf += n;
        i      += n;

        if (s_nbuf == ASR_SECTOR) {
            uint32_t base = s_got + i - ASR_SECTOR;
            bad = !flash_flush(base);
        }
    }
    if (!bad) s_got += len;
    else      s_state = ASR_IDLE;

    xSemaphoreGive(s_lock);

    if (bad) link_send_nack(PE_HTTP);
    else     link_send_ack(seq);
}

void asr_end(uint8_t seq)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    if (s_state == ASR_UPLOAD) {               /* END 的 ACK 丢了，F407 重发 */
        xSemaphoreGive(s_lock);
        link_send_ack(seq);
        return;
    }
    if (s_state != ASR_RECV || s_got == 0) {
        s_state = ASR_IDLE;
        s_nbuf  = 0;
        xSemaphoreGive(s_lock);
        link_send_ack(seq);
        link_send_nack(PE_ASR_EMPTY);
        return;
    }
    if (s_got != s_total) {
        uint32_t got = s_got;
        uint32_t expected = s_total;
        s_state = ASR_IDLE;
        s_nbuf  = 0;
        xSemaphoreGive(s_lock);
        ESP_LOGW(TAG, "END 长度不完整：收到 %u / %u，丢弃本轮", (unsigned)got, (unsigned)expected);
        link_send_ack(seq);
        link_send_nack(PE_BUSY);
        return;
    }

    uint32_t tail = s_got - s_nbuf;            /* 只整块落盘，差值必是扇区倍数 */
    bool bad = (s_nbuf > 0) && !flash_flush(tail);
    uint32_t got = s_got;
    int64_t  ms  = (esp_timer_get_time() - s_t_begin) / 1000;

    if (bad) {
        s_state = ASR_IDLE;
        s_nbuf  = 0;
        xSemaphoreGive(s_lock);
        link_send_ack(seq);
        link_send_nack(PE_HTTP);
        return;
    }
    s_state = ASR_UPLOAD;
    xSemaphoreGive(s_lock);

    link_send_ack(seq);                        /* 先放 F407 走，再开始上传 */
    ESP_LOGI(TAG, "收齐 %u 字节（报 %u），串口用了 %lld ms",
             (unsigned)got, (unsigned)s_total, ms);

    if (xTaskCreate(upload_task, "asr_up", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "上传任务创建失败");
        link_send_nack(PE_HTTP);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_pre   = 0;
        s_nbuf  = 0;
        s_state = ASR_IDLE;
        xSemaphoreGive(s_lock);
    }
}
