#pragma once
#include "esp_event.h"
#include "esp_netif_ip_addr.h"

// UI -> Service Actions
ESP_EVENT_DECLARE_BASE(UI_ACTION_BASE);
enum {
    UI_ACTION_WIFI_CONNECT,      // Payload: wifi_connect_req_t
    UI_ACTION_WIFI_DISCONNECT,   // Payload: NULL
    UI_ACTION_AUDIO_PLAY_SAMPLE, // Payload: NULL
    UI_ACTION_AUDIO_SET_VOLUME,  // Payload: int (0 - 100)
};

#define AUDIO_VOLUME_DEFAULT 60 // initial volume, shared by the audio service and the slider

typedef struct {
    char ssid[32];     // 1-32 bytes, not necessarily NUL-terminated
    char password[64]; // NUL-terminated, 8-63 characters (WPA2/WPA3 only)
} wifi_connect_req_t;

#define WIFI_PASS_MIN_LEN 8 // WPA2/WPA3 passphrase minimum, enforced by wifi_service

// Service -> UI / System Notifications
ESP_EVENT_DECLARE_BASE(SERVICE_STATUS_BASE);
enum {
    STATUS_WIFI_CONNECTING,
    STATUS_WIFI_CONNECTED, // Payload: esp_ip4_addr_t
    STATUS_WIFI_FAILED,       // Gave up after the reconnect window. Payload: int (wifi_err_reason_t)
    STATUS_WIFI_DISCONNECTED, // User-requested disconnect. Payload: NULL
    STATUS_WIFI_REJECTED,     // Invalid connect request, never sent to the radio. Payload: NULL
    STATUS_WIFI_ERROR,        // Wi-Fi driver call failed. Payload: esp_err_t
    STATUS_AUDIO_PLAYBACK_START,
    STATUS_AUDIO_PLAYBACK_IDLE,
};
