#pragma once

#include "esp_gmf_data_bus.h"

typedef void *audio_mixer_v2_handle_t;

typedef void (*audio_mixer_v2_out_cb_t)(void *ctx, uint8_t *data, int len);

typedef enum {
    AUDIO_VOLUME_NORMAL = 0,
    AUDIO_VOLUME_INCREASE = 1,
    AUDIO_VOLUME_DECREASE = 2,
} audio_volume_adjust_t;

#define DEFAULT_AUDIO_MIXER_V2_CONFIG() { \
    .out_cb = NULL, \
    .ctx = NULL, \
    .dst_sample_rate = 16000, \
    .dst_channel = 2, \
    .dst_bits = 32, \
    .src_sample_rate = 16000, \
    .src_channel = 1, \
    .src_bits = 16, \
    .nb_streams = 2, \
}

typedef struct {
    audio_mixer_v2_out_cb_t out_cb;
    void *ctx;
    int dst_sample_rate;
    int dst_channel;
    int dst_bits;
    int src_sample_rate;
    int src_channel;
    int src_bits;
    int nb_streams;
    esp_gmf_db_handle_t *bus;
} audio_mixer_v2_cfg_t;

esp_err_t audio_mixer_v2_new(audio_mixer_v2_cfg_t *cfg, audio_mixer_v2_handle_t *handle);

esp_err_t audio_mixer_v2_start(audio_mixer_v2_handle_t handle);

esp_err_t audio_mixer_v2_stop(audio_mixer_v2_handle_t handle);

esp_err_t audio_mixer_v2_destroy(audio_mixer_v2_handle_t handle);

esp_err_t audio_mixer_v2_set_volume_adjust(audio_mixer_v2_handle_t handle, esp_gmf_db_handle_t bus, audio_volume_adjust_t adjust);
