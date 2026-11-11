idf_component_register(
    SRCS "ui_manager.c" "ui_events.c" "ui_common.c" "screens/scr_home.c" "screens/scr_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board lvgl__lvgl
    PRIV_REQUIRES esp_event esp_netif espressif__esp_lvgl_port
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/main" "${CMAKE_CURRENT_SOURCE_DIR}")
