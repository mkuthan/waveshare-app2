#include <stdio.h>
#include "app_events.h"
#include "ui_common.h"
#include "ui_manager.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_wifi_label;
static lv_obj_t *s_audio_label;
static lv_obj_t *s_volume_label;

static void on_wifi_clicked(lv_event_t *e)
{
    scr_wifi_show();
}

static void on_play_clicked(lv_event_t *e)
{
    ui_post_action(UI_ACTION_AUDIO_PLAY_SAMPLE, NULL, 0);
}

static void on_volume_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    int vol = (int)lv_slider_get_value(slider);
    lv_label_set_text_fmt(s_volume_label, "Volume %d%%", vol);
    // Post once the finger is lifted, not on every drag step
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        ui_post_action(UI_ACTION_AUDIO_SET_VOLUME, &vol, sizeof(vol));
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

lv_obj_t *scr_home_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(s_screen, 24, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_screen, 20, 0);

    make_label(s_screen, "ESP32-P4", &lv_font_montserrat_28, COLOR_TEXT);
    s_wifi_label = make_label(s_screen, "Wi-Fi: not connected", &lv_font_montserrat_20, COLOR_MUTED);
    ui_make_button(s_screen, "Wi-Fi settings", COLOR_ACCENT, on_wifi_clicked);

    s_audio_label = make_label(s_screen, "Audio: idle", &lv_font_montserrat_20, COLOR_MUTED);
    ui_make_button(s_screen, "Play test tone", COLOR_ACCENT, on_play_clicked);

    s_volume_label = make_label(s_screen, "", &lv_font_montserrat_20, COLOR_TEXT);
    lv_obj_t *slider = lv_slider_create(s_screen);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 24);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, AUDIO_VOLUME_DEFAULT, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, on_volume_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, on_volume_changed, LV_EVENT_RELEASED, NULL);
    lv_label_set_text_fmt(s_volume_label, "Volume %d%%", AUDIO_VOLUME_DEFAULT);
    return s_screen;
}

void scr_home_show(void)
{
    lv_screen_load(s_screen);
}

void scr_home_update_wifi_status(bool connected)
{
    lv_label_set_text(s_wifi_label, connected ? "Wi-Fi: connected" : "Wi-Fi: not connected");
    lv_obj_set_style_text_color(s_wifi_label, connected ? COLOR_OK : COLOR_MUTED, 0);
}

void scr_home_update_audio_status(bool playing)
{
    lv_label_set_text(s_audio_label, playing ? "Audio: playing" : "Audio: idle");
    lv_obj_set_style_text_color(s_audio_label, playing ? COLOR_OK : COLOR_MUTED, 0);
}
