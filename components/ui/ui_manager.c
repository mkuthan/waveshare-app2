#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "bsp_board.h"
#include "ui_manager.h"

static const char *TAG = "ui_manager";

#define UI_LVGL_TASK_CORE      1
#define UI_LVGL_TASK_PRIORITY  4

// Direct mode + avoid_tearing scan out uninitialised PSRAM until the first frame is flushed, so
// the backlight stays off until a refresh cycle has completed. The port's DSI flush callback
// blocks until the buffer swap, hence REFR_READY means the home screen is actually on the panel.
static void on_first_refresh(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_target(e);
    lv_display_remove_event_cb_with_user_data(disp, on_first_refresh, NULL);
    esp_err_t err = bsp_display_set_brightness(100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "backlight on: %s", esp_err_to_name(err));
    }
}

esp_err_t ui_manager_init(void)
{
    esp_lcd_panel_handle_t panel = bsp_display_get_panel();
    esp_lcd_touch_handle_t touch = bsp_touch_get_handle();
    ESP_RETURN_ON_FALSE(panel && touch, ESP_ERR_INVALID_STATE, TAG, "display/touch not initialised");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = UI_LVGL_TASK_PRIORITY;
    port_cfg.task_affinity = UI_LVGL_TASK_CORE;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl port init");

    // Render straight into the panel's two PSRAM DPI framebuffers (avoid_tearing + direct mode)
    // instead of separate LVGL buffers: swapping the buffers the DSI peripheral scans out
    // removes both a full-frame copy through the bus and visible tearing.
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = bsp_display_get_io(),
        .panel_handle = panel,
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .swap_bytes = false,
            .direct_mode = true,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags.avoid_tearing = true,
    };
    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "add display");

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_cfg), ESP_FAIL, TAG, "add touch");

    ESP_RETURN_ON_FALSE(lvgl_port_lock(0), ESP_FAIL, TAG, "lvgl lock");
    scr_home_create();
    scr_wifi_create();
    scr_home_show();
    // Registered under the lock, after the screen is loaded, so the first event it sees is the
    // refresh that draws the home screen (not an empty one from before the screens existed).
    lv_display_add_event_cb(disp, on_first_refresh, LV_EVENT_REFR_READY, NULL);
    lvgl_port_unlock();

    return ESP_OK;
}
