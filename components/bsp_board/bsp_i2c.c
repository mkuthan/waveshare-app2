#include "driver/i2c_master.h"
#include "esp_check.h"
#include "bsp_board.h"
#include "bsp_pins.h"

// GT911, ES8311 and ES7210 all sit on GPIO 7/8. A second bus driver on the same pins would
// fight over them, so every slave attaches to this one handle (the driver serialises access).
static i2c_master_bus_handle_t s_i2c_bus = NULL;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA_PIN,
        .scl_io_num = BSP_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return s_i2c_bus;
}
