#pragma once
#include "esp_err.h"

/**
 * @brief Start Wi-Fi (ESP32-C6 over SDIO via esp_wifi_remote) and listen for UI_ACTION_WIFI_*.
 *
 * Requires the default event loop and NVS to be initialised. Progress is reported on
 * SERVICE_STATUS_BASE (STATUS_WIFI_*). Only WPA2/WPA3 networks are accepted. The last accepted
 * network is stored in NVS and rejoined on boot.
 */
esp_err_t wifi_service_init(void);
