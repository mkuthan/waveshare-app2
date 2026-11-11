# Waveshare ESP32 P4 Application

Touch UI, audio playback and Wi-Fi on the Waveshare ESP32-P4-WiFi6-Touch-LCD-4.3
(ESP-IDF v6.1). Board details: <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4.3>

## Build & flash

```sh
source "$HOME/.espressif/tools/activate_idf_v6.1.sh"
idf.py build
idf.py -p <port> flash monitor
```

## Architecture

`main/` brings the board up in order (NVS → default event loop → `bsp_board` → services →
UI) and owns the event contract in `main/app_events.h`. Components never include each
other's headers; they only post and subscribe on `esp_event`.

```
┌────────────────────────────────────────────────────────────────────────────┐
│ ui/            LVGL 9 · ui_manager · ui_events                             │
│                screens/scr_*.c                                             │
└────────────────────────────────────────────────────────────────────────────┘
         │                             ▲                                │
         │ UI_ACTION_BASE              │ SERVICE_STATUS_BASE            │
         │ (ui → service commands)     │ (service → ui state)           │
         ▼                             │                                │
┌──────────────────────────────────────────────────────────────┐        │
│ wifi_service/                                                │        │
│ ESP32-C6 over SDIO (esp_hosted)                              │        │
│ reconnect window · NVS credentials                           │        │
├──────────────────────────────────────────────────────────────┤        │
│ audio_service/                                               │        │
│ playback task · volume · amplifier gating                    │        │
└──────────────────────────────────────────────────────────────┘        │
                    │ bsp_audio_*            bsp_display_*, bsp_touch_* │
                    ▼                                                   ▼
┌────────────────────────────────────────────────────────────────────────────┐
│ bsp_board/   I2C · MIPI-DSI (ST7701) · GT911 touch                         │
│              ES8311 + ES7210 codecs · NS4150B PA (GPIO 53)                 │
└────────────────────────────────────────────────────────────────────────────┘
```

| Path | Role |
| --- | --- |
| `main/` | `app_main` init order; `app_events.h` is the only cross-component contract |
| `components/bsp_board/` | hardware only: I2C, ST7701 panel, GT911, codecs, speaker amp |
| `components/*_service/` | tasks and policy (Wi-Fi, audio) |
| `components/ui/` | `ui_manager`, `ui_events`, screens in `screens/scr_*.c` |
