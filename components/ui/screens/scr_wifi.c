#include <string.h>
#include <strings.h>
#include "app_events.h"
#include "ui_common.h"
#include "ui_manager.h"

static lv_obj_t *s_screen;
static lv_obj_t *s_ssid_ta;
static lv_obj_t *s_pass_ta;
static lv_obj_t *s_kb;
static lv_obj_t *s_status;

static void on_ta_focused(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target_obj(e);
    lv_keyboard_set_textarea(s_kb, ta);
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void on_kb_done(lv_event_t *e)
{
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void on_connect_clicked(lv_event_t *e)
{
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);
    wifi_connect_req_t req = {0};
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(pass);

    // Same limits the wifi service enforces; checked here so the user gets a message
    if (ssid_len == 0 || ssid_len > sizeof(req.ssid)) {
        scr_wifi_set_status("SSID must be 1-32 bytes");
        return;
    }
    if (pass_len < WIFI_PASS_MIN_LEN || pass_len >= sizeof(req.password)) {
        scr_wifi_set_status("Passphrase must be 8-63 characters");
        return;
    }
    memcpy(req.ssid, ssid, ssid_len);
    memcpy(req.password, pass, pass_len);

    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    esp_err_t err = ui_post_action(UI_ACTION_WIFI_CONNECT, &req, sizeof(req));
    explicit_bzero(&req, sizeof(req));
    if (err != ESP_OK) {
        scr_wifi_set_status("Busy, try again");
    }
}

static void on_disconnect_clicked(lv_event_t *e)
{
    ui_post_action(UI_ACTION_WIFI_DISCONNECT, NULL, 0);
}

static void on_back_clicked(lv_event_t *e)
{
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    scr_home_show();
}

static lv_obj_t *make_textarea(lv_obj_t *parent, const char *placeholder, uint32_t max_len, bool password)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_width(ta, lv_pct(100));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_max_length(ta, max_len);
    lv_textarea_set_password_mode(ta, password);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta, on_ta_focused, LV_EVENT_FOCUSED, NULL);
    return ta;
}

lv_obj_t *scr_wifi_create(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, COLOR_BG, 0);
    lv_obj_set_style_pad_all(s_screen, 24, 0);
    lv_obj_set_flex_flow(s_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_screen, 16, 0);

    lv_obj_t *title = lv_label_create(s_screen);
    lv_label_set_text(title, "Wi-Fi (2.4 GHz)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);

    s_ssid_ta = make_textarea(s_screen, "SSID", 32, false);
    s_pass_ta = make_textarea(s_screen, "Passphrase (WPA2/WPA3)", 63, true);

    s_status = lv_label_create(s_screen);
    lv_label_set_text(s_status, "Not connected");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_status, COLOR_MUTED, 0);

    ui_make_button(s_screen, "Connect", COLOR_ACCENT, on_connect_clicked);
    ui_make_button(s_screen, "Disconnect", COLOR_DANGER, on_disconnect_clicked);
    ui_make_button(s_screen, "Back", COLOR_MUTED, on_back_clicked);

    // Keyboard floats over the bottom of the screen and stays hidden until a field is focused
    s_kb = lv_keyboard_create(s_screen);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(s_kb, on_kb_done, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_kb, on_kb_done, LV_EVENT_CANCEL, NULL);
    return s_screen;
}

void scr_wifi_show(void)
{
    lv_screen_load(s_screen);
}

void scr_wifi_set_status(const char *text)
{
    lv_label_set_text(s_status, text);
}

void scr_wifi_clear_password(void)
{
    lv_textarea_set_text(s_pass_ta, "");
}
