#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief Start LVGL on the board display + touch and show the home screen.
 *
 * Requires bsp_board_init(). Switches the backlight on (from the LVGL task) once the first frame has been flushed.
 */
esp_err_t ui_manager_init(void);

/**
 * @brief Subscribe the UI to SERVICE_STATUS_BASE notifications (see ui_events.c).
 */
esp_err_t ui_events_init(void);

/* Screens. All of these except the *_create() calls must be made with the LVGL lock held
 * (lvgl_port_lock()); the *_create() calls are made by ui_manager_init() under the lock. */

lv_obj_t *scr_home_create(void);
void scr_home_show(void);
void scr_home_update_wifi_status(bool connected);
void scr_home_update_audio_status(bool playing);

lv_obj_t *scr_wifi_create(void);
void scr_wifi_show(void);
void scr_wifi_set_status(const char *text);
void scr_wifi_clear_password(void);
