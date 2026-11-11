#pragma once
#include "soc/gpio_num.h"

// Shared I2C (Touch + Audio Codecs)
#define BSP_I2C_SDA_PIN          GPIO_NUM_7
#define BSP_I2C_SCL_PIN          GPIO_NUM_8
#define BSP_I2C_NUM              0

// Audio
#define BSP_PA_EN_PIN            GPIO_NUM_53
#define BSP_I2S_NUM              0
#define BSP_I2S_MCLK             GPIO_NUM_13
#define BSP_I2S_BCLK             GPIO_NUM_12
#define BSP_I2S_WS               GPIO_NUM_10
#define BSP_I2S_DOUT             GPIO_NUM_9    // Playback (ES8311)
#define BSP_I2S_DIN              GPIO_NUM_11   // Record (ES7210)

// Display (ST7701, 2-lane MIPI-DSI)
#define BSP_LCD_RST_PIN          GPIO_NUM_27
#define BSP_LCD_BACKLIGHT_PIN    GPIO_NUM_26   // PWM with inverted output polarity (matches Waveshare BSP)
#define BSP_DSI_PHY_LDO_CHAN     3             // LDO_VO3 feeds VDD_MIPI_DPHY
#define BSP_DSI_PHY_LDO_MV       2500

// Touch (GT911), polled: neither line is wired for use
#define BSP_TOUCH_INT_PIN        GPIO_NUM_NC
#define BSP_TOUCH_RST_PIN        GPIO_NUM_NC
