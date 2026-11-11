#include "esp_check.h"
#include "bsp_board.h"

esp_err_t bsp_board_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), "bsp", "I2C init failed");
    ESP_RETURN_ON_ERROR(bsp_display_init(), "bsp", "display init failed");
    ESP_RETURN_ON_ERROR(bsp_touch_init(), "bsp", "touch init failed");
    ESP_RETURN_ON_ERROR(bsp_audio_codec_init(), "bsp", "audio codec init failed");
    return ESP_OK;
}
