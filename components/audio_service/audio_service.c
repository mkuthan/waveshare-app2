#include <math.h>
#include <stdatomic.h>
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "app_events.h"
#include "audio_service.h"
#include "bsp_board.h"

static const char *TAG = "audio_service";

#define AUDIO_RING_BYTES      (64 * 1024)
#define AUDIO_WRITE_BYTES     2048
#define AUDIO_IDLE_TIMEOUT_MS 300

// 441 Hz test tone: exactly 50 samples per period at the 22050 Hz BSP rate
#define TONE_PERIOD_SAMPLES   50
#define TONE_AMPLITUDE        12000
#define TONE_CHUNK_PERIODS    20
#define TONE_CHUNKS           22  // ~1 s
_Static_assert(BSP_AUDIO_SAMPLE_RATE_HZ == 441 * TONE_PERIOD_SAMPLES, "tone period assumes 441 Hz at the BSP rate");

// Event handlers run on the shared default-loop task, which must not block on codec I/O.
// So handlers only enqueue PCM into this PSRAM ring (PSRAM: keeps internal RAM free for
// Wi-Fi/DMA) and audio_task alone talks to the codec and the amplifier.
static RingbufHandle_t s_ring;
static esp_codec_dev_handle_t s_speaker;
static atomic_int s_volume = AUDIO_VOLUME_DEFAULT; // requested by the UI handler
static int s_applied_volume = -1;                   // audio_task only: -1 until first applied
static bool s_playing;                              // audio_task only
static int16_t s_chunk[TONE_PERIOD_SAMPLES * TONE_CHUNK_PERIODS];

static void post_status(int32_t id)
{
    esp_event_post(SERVICE_STATUS_BASE, id, NULL, 0, pdMS_TO_TICKS(50));
}

// audio_task only: the UI handler merely stores s_volume, so codec I/O stays on audio_task.
static void sync_volume(void)
{
    int vol = atomic_load(&s_volume);
    if (vol != s_applied_volume) {
        esp_codec_dev_set_out_vol(s_speaker, vol);
        s_applied_volume = vol;
    }
}

// The codec is already open (see audio_service_init), so playback only gates the amplifier.
static void playback_start(void)
{
    bsp_audio_pa_enable(true); // PA on only after the codec is running, to avoid a turn-on pop
    s_playing = true;
    post_status(STATUS_AUDIO_PLAYBACK_START);
}

static void playback_stop(void)
{
    s_playing = false;
    bsp_audio_pa_enable(false);
    post_status(STATUS_AUDIO_PLAYBACK_IDLE);
}

static void audio_task(void *arg)
{
    // The idle timeout (rather than "ring empty") ends playback so brief refill gaps don't
    // bounce the amplifier on and off, which is audible as clicks.
    for (;;) {
        size_t len = 0;
        void *buf = xRingbufferReceiveUpTo(s_ring, &len, pdMS_TO_TICKS(AUDIO_IDLE_TIMEOUT_MS), AUDIO_WRITE_BYTES);
        if (buf) {
            if (!s_playing) {
                playback_start();
            }
            sync_volume(); // picks up slider changes made while playing
            esp_codec_dev_write(s_speaker, buf, len);
            vRingbufferReturnItem(s_ring, buf);
        } else if (s_playing) {
            playback_stop();
        }
    }
}

static void queue_test_tone(void)
{
    // Whole chunks only: never leave a truncated tone in the ring if it is nearly full
    for (int i = 0; i < TONE_CHUNKS; i++) {
        if (xRingbufferSend(s_ring, s_chunk, sizeof(s_chunk), 0) != pdTRUE) {
            ESP_LOGW(TAG, "audio busy, sample truncated after %d chunks", i);
            return;
        }
    }
}

static void on_ui_action(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case UI_ACTION_AUDIO_PLAY_SAMPLE:
        queue_test_tone();
        break;
    case UI_ACTION_AUDIO_SET_VOLUME: {
        if (!data) {
            return;
        }
        int vol = *(const int *)data;
        vol = vol < 0 ? 0 : (vol > 100 ? 100 : vol);
        atomic_store(&s_volume, vol);
        break;
    }
    default:
        break; // wifi actions are not ours
    }
}

esp_err_t audio_service_init(void)
{
    s_speaker = bsp_audio_get_speaker();
    ESP_RETURN_ON_FALSE(s_speaker, ESP_ERR_INVALID_STATE, TAG, "speaker not initialised, call bsp_board_init() first");

    // Open the codec once and keep it open for the service lifetime: esp_codec_dev's I2S
    // backend disables the channel unconditionally when setting the format, so closing and
    // reopening it logs "the channel has not been enabled yet" and reruns the full I2S
    // setup on every play. Idle playback therefore gates only the amplifier.
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = BSP_AUDIO_SAMPLE_RATE_HZ,
        .channel = 1,
        .bits_per_sample = 16,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_speaker, &fs) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "codec open");
    sync_volume(); // hand the codec the slider's initial volume

    for (int i = 0; i < TONE_PERIOD_SAMPLES * TONE_CHUNK_PERIODS; i++) {
        s_chunk[i] = (int16_t)(TONE_AMPLITUDE * sinf(2.0f * (float)M_PI * (float)(i % TONE_PERIOD_SAMPLES) / TONE_PERIOD_SAMPLES));
    }

    s_ring = xRingbufferCreateWithCaps(AUDIO_RING_BYTES, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(s_ring, ESP_ERR_NO_MEM, TAG, "PSRAM ring buffer");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(UI_ACTION_BASE, ESP_EVENT_ANY_ID, on_ui_action, NULL), TAG, "reg ui");
    ESP_RETURN_ON_FALSE(xTaskCreate(audio_task, "audio", 4096, NULL, 5, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}
