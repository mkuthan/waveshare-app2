#include "esp_event.h"
#include "esp_log.h"
#include "app_events.h"
#include "ui_common.h"

static const char *TAG = "ui_common";

#define UI_POST_TIMEOUT_MS  100
#define UI_BUTTON_HEIGHT    72

lv_obj_t *ui_make_button(lv_obj_t *parent, const char *text, lv_color_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, lv_pct(100), UI_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    return btn;
}

esp_err_t ui_post_action(int32_t id, const void *data, size_t size)
{
    esp_err_t err = esp_event_post(UI_ACTION_BASE, id, data, size, pdMS_TO_TICKS(UI_POST_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "action %d not posted: %s", (int)id, esp_err_to_name(err));
    }
    return err;
}
