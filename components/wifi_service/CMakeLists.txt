idf_component_register(
    SRCS "wifi_service.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_event esp_netif esp_timer esp_wifi nvs_flash
)

# app_events.h lives in main/ (the shared event contract); main depends on this
# component so it can't be a REQUIRES, add its directory directly.
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/main")
