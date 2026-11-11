idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_event esp_netif bsp_board wifi_service audio_service ui
)
