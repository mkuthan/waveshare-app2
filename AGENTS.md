# AGENTS.md

## Board: Waveshare ESP32-P4-WiFi6-Touch-LCD-4.3

- Chip: esp32p4 (32MB PSRAM, 32MB NOR flash)
- Display: 4.3" IPS 480x800, ST7701, 2-lane MIPI-DSI
- Touch: GT911, I2C SDA 7 / SCL 8 (external pullups)
- Wi-Fi/BLE: ESP32-C6-MINI via SDIO (esp_wifi_remote, esp_hosted), 2.4 GHz only
- Camera: MIPI-CSI, OV5647 supported
- Audio: ES8311 codec, ES7210 AEC, dual mics, speaker amp
- I2S: MCLK 13, SCLK 12, ASDOUT 11, LRCK 10, DSDIN 9, PA_Ctrl 53 (active high)
- Storage: TF card, SDIO 3.0 (CLK 43, CMD 44, D0 39, D1 40, D2 41, D3 42)
- Buttons: BOOT (download mode), RESET, POWER
- USB-C: UART (power/flash/monitor) + USB OTG 2.0 HS
- Docs: <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-4.3>

## IDF

Activation: `source "$HOME/.espressif/tools/activate_idf_v6.1.sh"`

- idf.py build after changes; fix errors/warnings
- No flash/erase/monitor without approval

## Layout

- main/                  app_main init order; app_events.h = only cross-component contract
- components/bsp_board/  hardware only: I2C, ST7701 panel, GT911, codecs, PA; exports bsp_**get**()
- components/*_service/  tasks + policy (wifi, audio); speak only via esp_event
- components/ui/         ui_manager, ui_events, screens/scr_*.c

## Design principles

- Straightforward, idiomatic C/C++; readable by Python/JS programmer
- No metaprogramming/macro magic unless necessary
- Explicit data flow; pass inputs, avoid hidden global state
- YAGNI; no defensive wrappers or speculative abstractions
- ESP-IDF logging (not printf)
- Check esp_err_t explicitly
- No hardcoded/logged secrets; use Kconfig/NVS/secure storage
- Comments explain why (constraints, decisions), not what
- Remove stale comments
