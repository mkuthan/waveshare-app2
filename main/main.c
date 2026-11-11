#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "app_events.h"
#include "bsp_board.h"
#include "wifi_service.h"
#include "audio_service.h"
#include "ui_manager.h"

// Defined here because main owns the event contract (app_events.h). Components only
// post/listen on these bases and never include each other, so there are no cycles.
ESP_EVENT_DEFINE_BASE(UI_ACTION_BASE);
ESP_EVENT_DEFINE_BASE(SERVICE_STATUS_BASE);

void app_main(void)
{
    // Order matters: services and UI subscribe to the default loop (so it comes first), and
    // both need the BSP handles. The UI goes last so the first screen already reflects state.
    // 1. NVS Flash Initialization
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Default System Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Board Hardware Initialization (I2C, MIPI-DSI, touch, codecs, GPIO 53 PA)
    ESP_ERROR_CHECK(bsp_board_init());

    // 4. Background Services Initialization
    ESP_ERROR_CHECK(wifi_service_init());
    ESP_ERROR_CHECK(audio_service_init());

    // 5. LVGL 9 UI Initialization & Event Subscriptions
    ESP_ERROR_CHECK(ui_manager_init());
    ESP_ERROR_CHECK(ui_events_init());

    ESP_LOGI("APP", "System initialized successfully.");
}
