#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "bsp_pins.h"

static const char *TAG = "bsp_audio";

static i2s_chan_handle_t s_i2s_tx = NULL;
static i2s_chan_handle_t s_i2s_rx = NULL;
static esp_codec_dev_handle_t s_speaker = NULL;
static esp_codec_dev_handle_t s_mic = NULL;

static esp_err_t pa_gpio_init(void)
{
    // Pull-down + low: the NS4150B must stay off until audio_service asks for it
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BSP_PA_EN_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "PA gpio");
    return gpio_set_level(BSP_PA_EN_PIN, 0);
}

esp_err_t bsp_audio_pa_enable(bool enable)
{
    return gpio_set_level(BSP_PA_EN_PIN, enable ? 1 : 0);
}

static esp_err_t i2s_duplex_init(const audio_codec_data_if_t **out_data_if)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // play silence, not stale DMA data, on underrun
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_i2s_tx, &s_i2s_rx), TAG, "I2S channel");

    const i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_BCLK,
            .ws = BSP_I2S_WS,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DIN,
        },
    };
    esp_err_t ret = i2s_channel_init_std_mode(s_i2s_tx, &std_cfg);
    if (ret == ESP_OK) {
        ret = i2s_channel_init_std_mode(s_i2s_rx, &std_cfg);
    }
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(s_i2s_tx);
    }
    if (ret == ESP_OK) {
        ret = i2s_channel_enable(s_i2s_rx);
    }
    if (ret != ESP_OK) {
        i2s_del_channel(s_i2s_tx);
        i2s_del_channel(s_i2s_rx);
        s_i2s_tx = s_i2s_rx = NULL;
        return ret;
    }

    audio_codec_i2s_cfg_t data_cfg = {
        .port = BSP_I2S_NUM,
        .rx_handle = s_i2s_rx,
        .tx_handle = s_i2s_tx,
    };
    *out_data_if = audio_codec_new_i2s_data(&data_cfg);
    return *out_data_if ? ESP_OK : ESP_FAIL;
}

static esp_codec_dev_handle_t new_codec_dev(esp_codec_dev_type_t type, const audio_codec_if_t *codec_if,
                                            const audio_codec_data_if_t *data_if)
{
    esp_codec_dev_cfg_t cfg = {
        .dev_type = type,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    return esp_codec_dev_new(&cfg);
}

esp_err_t bsp_audio_codec_init(void)
{
    if (s_speaker && s_mic) {
        return ESP_OK;
    }
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "I2C bus not initialised");

    ESP_RETURN_ON_ERROR(pa_gpio_init(), TAG, "PA init");

    const audio_codec_data_if_t *data_if = NULL;
    ESP_RETURN_ON_ERROR(i2s_duplex_init(&data_if), TAG, "I2S init");

    // ES8311 (DAC / playback) on the shared I2C bus
    audio_codec_i2c_cfg_t es8311_i2c = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bus,
    };
    const audio_codec_ctrl_if_t *es8311_ctrl = audio_codec_new_i2c_ctrl(&es8311_i2c);
    ESP_RETURN_ON_FALSE(es8311_ctrl, ESP_FAIL, TAG, "ES8311 ctrl");
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = es8311_ctrl,
        .gpio_if = audio_codec_new_gpio(),
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = -1, // PA is driven by bsp_audio_pa_enable()
        .use_mclk = true,
        .hw_gain = { .pa_voltage = 5.0, .codec_dac_voltage = 3.3 },
    };
    const audio_codec_if_t *es8311 = es8311_codec_new(&es8311_cfg);
    ESP_RETURN_ON_FALSE(es8311, ESP_FAIL, TAG, "ES8311 codec");
    s_speaker = new_codec_dev(ESP_CODEC_DEV_TYPE_OUT, es8311, data_if);
    ESP_RETURN_ON_FALSE(s_speaker, ESP_FAIL, TAG, "speaker dev");

    // ES7210 (ADC / dual mic) on the same bus
    audio_codec_i2c_cfg_t es7210_i2c = {
        .port = BSP_I2C_NUM,
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = bus,
    };
    const audio_codec_ctrl_if_t *es7210_ctrl = audio_codec_new_i2c_ctrl(&es7210_i2c);
    ESP_RETURN_ON_FALSE(es7210_ctrl, ESP_FAIL, TAG, "ES7210 ctrl");
    es7210_codec_cfg_t es7210_cfg = { .ctrl_if = es7210_ctrl };
    const audio_codec_if_t *es7210 = es7210_codec_new(&es7210_cfg);
    ESP_RETURN_ON_FALSE(es7210, ESP_FAIL, TAG, "ES7210 codec");
    s_mic = new_codec_dev(ESP_CODEC_DEV_TYPE_IN, es7210, data_if);
    ESP_RETURN_ON_FALSE(s_mic, ESP_FAIL, TAG, "mic dev");

    ESP_LOGI(TAG, "ES8311 + ES7210 ready, PA off");
    return ESP_OK;
}

esp_codec_dev_handle_t bsp_audio_get_speaker(void)
{
    return s_speaker;
}

esp_codec_dev_handle_t bsp_audio_get_mic(void)
{
    return s_mic;
}
