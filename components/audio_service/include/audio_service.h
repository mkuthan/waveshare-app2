#pragma once
#include "esp_err.h"

/**
 * @brief Start the playback task and listen for UI_ACTION_AUDIO_*.
 *
 * Requires bsp_board_init() and the default event loop. The speaker amplifier is only
 * powered while audio is playing; STATUS_AUDIO_PLAYBACK_START/IDLE bracket that window.
 */
esp_err_t audio_service_init(void);
