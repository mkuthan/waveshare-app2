idf_component_register(
    SRCS "bsp_board.c" "bsp_i2c.c" "bsp_display.c" "bsp_audio_codec.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio esp_driver_i2s esp_lcd espressif__esp_lcd_touch espressif__esp_codec_dev
    PRIV_REQUIRES esp_driver_ledc esp_mm espressif__esp_lcd_st7701 espressif__esp_lcd_touch_gt911
)
