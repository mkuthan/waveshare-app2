#include <stdio.h>
#include "esp_event.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "app_events.h"
#include "ui_manager.h"

static void on_service_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    char text[64];

    // We run on the default event loop task, not the LVGL task, and LVGL is not thread safe:
    // every widget touch from here must hold the port lock.
    lvgl_port_lock(0);
    switch (id) {
    case STATUS_WIFI_CONNECTING:
        scr_home_update_wifi_status(false);
        scr_wifi_set_status("Connecting...");
        break;
    case STATUS_WIFI_CONNECTED: {
        char ip[16];
        esp_ip4addr_ntoa((const esp_ip4_addr_t *)data, ip, sizeof(ip));
        snprintf(text, sizeof(text), "Connected, IP %s", ip);
        scr_home_update_wifi_status(true);
        scr_wifi_set_status(text);
        scr_wifi_clear_password();
        break;
    }
    case STATUS_WIFI_FAILED:
        snprintf(text, sizeof(text), "Connection failed (Wi-Fi reason %d)", data ? *(const int *)data : -1);
        scr_home_update_wifi_status(false);
        scr_wifi_set_status(text);
        break;
    case STATUS_WIFI_DISCONNECTED:
        scr_home_update_wifi_status(false);
        scr_wifi_set_status("Disconnected");
        break;
    case STATUS_WIFI_REJECTED:
        scr_home_update_wifi_status(false);
        scr_wifi_set_status("Need an SSID and an 8-63 char password");
        break;
    case STATUS_WIFI_ERROR:
        snprintf(text, sizeof(text), "Wi-Fi error: %s", data ? esp_err_to_name(*(const esp_err_t *)data) : "unknown");
        scr_home_update_wifi_status(false);
        scr_wifi_set_status(text);
        break;
    case STATUS_AUDIO_PLAYBACK_START:
        scr_home_update_audio_status(true);
        break;
    case STATUS_AUDIO_PLAYBACK_IDLE:
        scr_home_update_audio_status(false);
        break;
    default:
        break;
    }
    lvgl_port_unlock();
}

esp_err_t ui_events_init(void)
{
    return esp_event_handler_register(SERVICE_STATUS_BASE, ESP_EVENT_ANY_ID, on_service_event, NULL);
}
