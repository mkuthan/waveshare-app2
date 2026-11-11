#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_codec_dev.h"

#define BSP_LCD_H_RES            480
#define BSP_LCD_V_RES            800

// Default I2S clock of the audio port. Callers opening the codec at another rate make the
// driver reconfigure the clock, so they should use this one.
#define BSP_AUDIO_SAMPLE_RATE_HZ 22050

/**
 * @brief Bring up I2C, display (+ backlight off), touch and audio codecs, in that order.
 */
esp_err_t bsp_board_init(void);

// Shared I2C master bus (GPIO 7/8). One bus only: all slaves attach to this handle.
esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

// Display: panel is created with two PSRAM-backed DPI framebuffers
esp_err_t bsp_display_init(void);
esp_lcd_panel_handle_t bsp_display_get_panel(void);
esp_lcd_panel_io_handle_t bsp_display_get_io(void);
esp_err_t bsp_display_set_brightness(int percent);

// Touch: GT911 in polling mode, attached to the shared I2C bus
esp_err_t bsp_touch_init(void);
esp_lcd_touch_handle_t bsp_touch_get_handle(void);

// Audio: ES8311 (playback) + ES7210 (capture) on one duplex I2S port
esp_err_t bsp_audio_codec_init(void);
esp_codec_dev_handle_t bsp_audio_get_speaker(void);
esp_codec_dev_handle_t bsp_audio_get_mic(void);
// NS4150B amplifier (GPIO 53, active high). Off after init.
esp_err_t bsp_audio_pa_enable(bool enable);
