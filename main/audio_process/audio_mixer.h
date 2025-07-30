#pragma once

#include "esp_err.h"
#include "esp_gmf_data_bus.h"

typedef enum {
    AUDIO_MIXER_EVENT_STARTED = 0,
    AUDIO_MIXER_EVENT_STOPPED,
    AUDIO_MIXER_EVENT_DATA_PROCESSED,
    AUDIO_MIXER_EVENT_DESTROYED,
} audio_mixer_event_t;

#define DEFAULT_AUDIO_MIXER_CONFIG() { \
    .callback = NULL, \
    .arg = NULL, \
    .dst_sample_rate = 16000, \
    .dst_channel = 1, \
    .dst_bits_per_sample = 16, \
}

typedef void (*audio_mixer_callback_t)(void *arg, audio_mixer_event_t event, char *samples, int sample_bytes);

typedef struct {
    audio_mixer_callback_t callback;
    void *arg;
    int dst_sample_rate;
    int dst_channel;
    int dst_bits_per_sample;
} audio_mixer_cfg_t;

typedef void *audio_mixer_handle_t;

esp_err_t audio_mixer_new(audio_mixer_handle_t *handle, audio_mixer_cfg_t *cfg);

esp_err_t audio_mixer_destroy(audio_mixer_handle_t handle);

esp_err_t audio_mixer_add_stream(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus);

esp_err_t audio_mixer_remove_stream(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus);

esp_err_t audio_mixer_start(audio_mixer_handle_t handle);

esp_err_t audio_mixer_stop(audio_mixer_handle_t handle);
