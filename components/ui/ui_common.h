#pragma once
#include "esp_err.h"
#include "lvgl.h"

// Styling and helpers shared by the screens; private to the ui component.

#define COLOR_BG      lv_color_hex(0x10141a)
#define COLOR_TEXT    lv_color_hex(0xe6e9ee)
#define COLOR_MUTED   lv_color_hex(0x8b94a3)
#define COLOR_ACCENT  lv_color_hex(0x2d7ff9)
#define COLOR_OK      lv_color_hex(0x3ecf8e)
#define COLOR_DANGER  lv_color_hex(0xd9534f)

// Full-width button with a centred label. Call with the LVGL lock held.
lv_obj_t *ui_make_button(lv_obj_t *parent, const char *text, lv_color_t color, lv_event_cb_t cb);

// Post a UI_ACTION_* event without blocking the LVGL task for long; logs when it can't.
esp_err_t ui_post_action(int32_t id, const void *data, size_t size);
