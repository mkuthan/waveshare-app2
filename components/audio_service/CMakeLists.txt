idf_component_register(
    SRCS "audio_service.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board
    PRIV_REQUIRES esp_event esp_netif esp_ringbuf espressif__esp_codec_dev
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/main")
