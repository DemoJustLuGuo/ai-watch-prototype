/* 节选自 c3/main/link.c —— ESP32-C3 侧帧接收任务与 UART 初始化 */

static void link_task(void *arg)
{
    uint8_t byte;
    proto_frame_t frame;
    uint8_t crc_err;

    for (;;) {
        int n = uart_read_bytes(LINK_UART, &byte, 1, pdMS_TO_TICKS(1000));
        if (n <= 0) continue;

        if (proto_feed(byte, &frame, &crc_err)) {
            handle_frame(&frame);
        } else if (crc_err) {
            uint8_t err = PE_CRC;
            ESP_LOGW(TAG, "CRC 错，回 NACK");
            send_frame(PT_NACK, s_seq++, &err, 1);
        }
    }
}

void link_start(void)
{
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX_PIN, LINK_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(link_task, "link", 8192, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART%d  %d 8N1  TX=GPIO%d RX=GPIO%d",
             LINK_UART, LINK_BAUD, LINK_TX_PIN, LINK_RX_PIN);
}
