idf_component_register(
    SRCS "main.c" "proto.c" "link.c" "wifi_mgr.c" "weather.c" "asr.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_client esp-tls json driver
             esp_timer esp_partition esp_app_format app_update
)
