#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"

#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_rate_cvt.h"


#include "gmf_loader_setup_defaults.h"
#include "esp_gmf_new_databus.h"

#include "esp_ae_mixer.h"

// #include "esp_gmf_rate_cvt.h"
// #include "esp_gmf_bit_cvt.h"
// #include "esp_gmf_ch_cvt.h"

#include "esp_gmf_oal_mutex.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"

#include "audio_mixer.h"

#define AUDIO_MAX_MIXER_NUM          2
#define AUDIO_MIXER_SAMPLE_NUM       512
#define AUDIO_MIXER_SAMPLE_NUM_BYTES (AUDIO_MIXER_SAMPLE_NUM * sizeof(char) * 2)

#define EVENT_GROUP_START    (1 << 0)
#define EVENT_GROUP_DESTROY  (1 << 1)

static const char *TAG = "AUDIO_MIXER";

typedef struct {
    void *mixer_handle;
    audio_mixer_cfg_t cfg;
    EventGroupHandle_t event_group;
    esp_gmf_oal_thread_t thread;
    esp_gmf_db_handle_t playback_bus;
    esp_gmf_db_handle_t bus[AUDIO_MAX_MIXER_NUM];
    esp_ae_sample_t in_samples[AUDIO_MAX_MIXER_NUM];
    esp_ae_sample_t out_samples;
    bool process_running;
    esp_gmf_pool_handle_t default_pool;
    esp_gmf_pool_handle_t pool;
    esp_gmf_pipeline_handle_t pipe;
    esp_gmf_task_handle_t task;
} audio_mixer_t;

#include <stdio.h>
#include <string.h>


static void audio_mixer_process(audio_mixer_t *mixer)
{
    int k = 0;
    int valid_slot_mask = 0;
    for (int i = 0; i < AUDIO_MAX_MIXER_NUM; i++) {
        if (mixer->bus[i] == NULL) {
            continue;
        }
        k++;
        valid_slot_mask |= (1 << i);
        ESP_LOGD(TAG, "Valid slot mask: %b", valid_slot_mask);
        memset(mixer->in_samples[i], 0, AUDIO_MIXER_SAMPLE_NUM_BYTES);
        esp_gmf_data_bus_block_t blk = {0};
        blk.buf_length = AUDIO_MIXER_SAMPLE_NUM_BYTES;
        blk.buf = (uint8_t *)mixer->in_samples[i];
        esp_gmf_db_acquire_read(mixer->bus[i], &blk, blk.buf_length, pdMS_TO_TICKS(0));
        esp_gmf_db_release_read(mixer->bus[i], &blk, pdMS_TO_TICKS(0));
        // printf("%s | %d blk.valid_size: %d (slot: %d)\n", __func__, __LINE__, blk.valid_size, i);

    }
    switch (k) {
        case 0: {
            audio_mixer_stop(mixer);
            mixer->cfg.callback(mixer->cfg.arg, AUDIO_MIXER_EVENT_STOPPED, "Invalid data", strlen("Invalid data"));
            return;
        }
        case 1: {
            int slot = 0;
            for (; slot < AUDIO_MAX_MIXER_NUM; slot++) {
                if (valid_slot_mask & (1 << slot)) {
                    ESP_LOGD(TAG, "Valid slot found: %d", slot);
                    break;
                } else {
                    continue;
                }
            }
            memcpy(mixer->out_samples, mixer->in_samples[slot], AUDIO_MIXER_SAMPLE_NUM_BYTES);
            break;
        }
        case 2: {
            esp_ae_mixer_process(mixer->mixer_handle, AUDIO_MIXER_SAMPLE_NUM, (esp_ae_sample_t *)mixer->in_samples, mixer->out_samples);
            break;
        }
    }
    esp_gmf_data_bus_block_t rb_blk = {0};
    int ret = esp_gmf_db_acquire_write(mixer->playback_bus, &rb_blk, AUDIO_MIXER_SAMPLE_NUM_BYTES, pdMS_TO_TICKS(5));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire write to playback FIFO (0x%x)", ret);
        return;
    }
    rb_blk.valid_size = AUDIO_MIXER_SAMPLE_NUM_BYTES;
    rb_blk.buf = (uint8_t *)mixer->out_samples;
    ret = esp_gmf_db_release_write(mixer->playback_bus, &rb_blk, pdMS_TO_TICKS(5));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to release write to playback FIFO (0x%x)", ret);
        return;
    }
    // mixer->cfg.callback(mixer->cfg.arg, AUDIO_MIXER_EVENT_DATA_PROCESSED, mixer->out_samples, AUDIO_MIXER_SAMPLE_NUM_BYTES);
    return;
}

static void audio_mixer_thread(void *arg)
{
    audio_mixer_t *mixer = (audio_mixer_t *)arg;
    mixer->process_running = true;
    while (1) {
        printf("%s | %d\n", __func__, __LINE__);
        EventBits_t bits = xEventGroupWaitBits(mixer->event_group, EVENT_GROUP_START | EVENT_GROUP_DESTROY, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & EVENT_GROUP_DESTROY) {
            break;
        }
        while (mixer->process_running) {
            audio_mixer_process(mixer);
        }
    }
    vTaskDelete(NULL);
}


static esp_err_t mixer_pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGD(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return 0;
}

static int mixer_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{    
    return 0;
}

static int mixer_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    mixer->cfg.callback(mixer->cfg.arg, AUDIO_MIXER_EVENT_DATA_PROCESSED, mixer->out_samples, AUDIO_MIXER_SAMPLE_NUM_BYTES);
    return 0;
}

static int mixer_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;

    esp_gmf_data_bus_block_t _blk = {0};
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;
    int ret = esp_gmf_db_acquire_read(mixer->playback_bus, &_blk, wanted_size, block_ticks);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
        return ESP_FAIL;
    }
    blk->valid_size = _blk.valid_size;
    esp_gmf_db_release_read(mixer->playback_bus, &_blk, block_ticks);
    return wanted_size;
}

static int mixer_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return blk->valid_size;
}

static esp_err_t audio_mixer_link_pipeline(audio_mixer_t *mixer)
{
    esp_err_t err = ESP_OK;
    const char *elements[] = {"aud_rate_cvt", "aud_ch_cvt", "aud_bit_cvt"};
    err = esp_gmf_pool_new_pipeline(mixer->pool, NULL, elements, sizeof(elements) / sizeof(elements[0]), NULL, &mixer->pipe);
    
    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_rate_cvt", &rate_cvt);
    if (rate_cvt) {
        esp_gmf_rate_cvt_set_dest_rate(rate_cvt, mixer->cfg.dst_sample_rate);
        printf("mixer->cfg.dst_sample_rate: %d\n", mixer->cfg.dst_sample_rate);
    } else {
        ESP_LOGE(TAG, "Failed to get rate_cvt");
        return ESP_FAIL;
    }

    esp_gmf_element_handle_t ch_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_ch_cvt", &ch_cvt);
    if (ch_cvt) {
        esp_gmf_ch_cvt_set_dest_channel(ch_cvt, mixer->cfg.dst_channel);
        printf("mixer->cfg.dst_channel: %d\n", mixer->cfg.dst_channel);
    } else {
        ESP_LOGE(TAG, "Failed to get ch_cvt");
        return ESP_FAIL;
    }
    
    esp_gmf_element_handle_t bit_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_bit_cvt", &bit_cvt);
    if (bit_cvt) {
        esp_gmf_bit_cvt_set_dest_bits(bit_cvt, mixer->cfg.dst_bits_per_sample);
        printf("mixer->cfg.dst_bits_per_sample: %d\n", mixer->cfg.dst_bits_per_sample);
    } else {
        ESP_LOGE(TAG, "Failed to get bit_cvt");
        return ESP_FAIL;
    }
    
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        mixer_outport_acquire_write,
        mixer_outport_release_write,
        NULL, (void *)mixer, 2048, portMAX_DELAY);
    
    esp_err_t ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, elements[2], ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register output port");
    
    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        mixer_inport_acquire_read,
        mixer_inport_release_read,
        NULL, (void *)mixer, 2048, portMAX_DELAY);
    
    ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, elements[0], ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register input port");
    
    esp_gmf_info_sound_t req_info = {
        .sample_rates = 16000,
        .channels = 1,
        .bits = 16,
    };
    esp_gmf_pipeline_report_info(mixer->pipe, ESP_GMF_INFO_SOUND, &req_info, sizeof(req_info));

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    // cfg.thread.core = AUDIO_RECORD_PIP_TASK_CORE;
    cfg.thread.prio = 20;
    // cfg.thread.stack = AUDIO_RECORD_PIP_TASK_STACK_SIZE;
    cfg.name = "mixer_playback_task";
    
    ret = esp_gmf_task_init(&cfg, &mixer->task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create recorder task");
        return ESP_FAIL;
    }
    esp_gmf_pipeline_bind_task(mixer->pipe, mixer->task);
    esp_gmf_pipeline_loading_jobs(mixer->pipe);
    esp_gmf_pipeline_set_event(mixer->pipe, mixer_pipeline_event, NULL);
    esp_gmf_task_set_timeout(mixer->task, 3000);
    ret = esp_gmf_pipeline_run(mixer->pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to run mixer pipeline");
     
    return err;
}

esp_err_t audio_mixer_new(audio_mixer_handle_t *handle, audio_mixer_cfg_t *cfg)
{
    *handle = NULL;
    audio_mixer_t *mixer = (audio_mixer_t *)esp_gmf_oal_calloc(1, sizeof(audio_mixer_t));
    if (!mixer) {
        return ESP_ERR_NO_MEM;
    }
    mixer->cfg = *cfg;

    mixer->out_samples = (char *)esp_gmf_oal_malloc_align(16, AUDIO_MIXER_SAMPLE_NUM_BYTES);
    if (!mixer->out_samples) {
        return ESP_ERR_NO_MEM;
    }

    esp_ae_mixer_info_t source_info[AUDIO_MAX_MIXER_NUM] = {0};
    esp_ae_mixer_info_t info1 = {
        .weight1 = 0.5,
        .weight2 = 1.0,
        .transit_time = 3000,
    };
    source_info[0] = info1;
    esp_ae_mixer_info_t info2 = {
        .weight1 = 0.0,
        .weight2 = 0.5,
        .transit_time = 6000,
    };
    source_info[1] = info2;
    esp_ae_mixer_cfg_t downmix_cfg;
    downmix_cfg.sample_rate = 16000;
    downmix_cfg.channel = 1;
    downmix_cfg.bits_per_sample = 16;
    downmix_cfg.src_info = source_info;
    downmix_cfg.src_num = AUDIO_MAX_MIXER_NUM;

    int ret = esp_gmf_db_new_ringbuf(1, 4096, &mixer->playback_bus);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    ret = esp_ae_mixer_open(&downmix_cfg, &mixer->mixer_handle);
    if (ret != ESP_AE_ERR_OK) {
        goto cleanup;
    }

    esp_ae_mixer_set_mode(mixer->mixer_handle, 0, ESP_AE_MIXER_MODE_FADE_UPWARD);
    esp_ae_mixer_set_mode(mixer->mixer_handle, 1, ESP_AE_MIXER_MODE_FADE_UPWARD);
// Create a pool
    ret = esp_gmf_pool_init(&mixer->pool);

    esp_gmf_element_handle_t hd = NULL;
    esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    esp_gmf_ch_cvt_init(&ch_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    esp_gmf_bit_cvt_init(&bit_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    esp_gmf_rate_cvt_init(&rate_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    ret = audio_mixer_link_pipeline(mixer);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    mixer->event_group = xEventGroupCreate();
    if (!mixer->event_group) {
        goto cleanup;
    }
    esp_gmf_oal_thread_create(NULL, "audio_mixer_thread", audio_mixer_thread, (void *)mixer, 1024 * 10, 10, true, 0);
    *handle = (audio_mixer_handle_t)mixer;
    return ESP_OK;

cleanup:
    ESP_LOGE(TAG, "audio_mixer_new failed");
    if (mixer->out_samples) {
        esp_gmf_oal_free(mixer->out_samples);
    }
    if (mixer->playback_bus) {
        // esp_gmf_db_destroy(mixer->playback_bus);
    }
    if (mixer->mixer_handle) {
        esp_ae_mixer_close(mixer->mixer_handle);
    }
    if (mixer->pipe) {
        esp_gmf_pipeline_destroy(mixer->pipe);
    }
    esp_gmf_oal_free(mixer);
    return ESP_FAIL;
}

esp_err_t audio_mixer_destroy(audio_mixer_handle_t handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    vEventGroupDelete(mixer->event_group);
    esp_gmf_oal_thread_delete(mixer->thread);
    esp_ae_mixer_close(mixer->mixer_handle);
    for (int i = 0; i < AUDIO_MAX_MIXER_NUM; i++) {
        if (mixer->in_samples[i]) {
            esp_gmf_oal_free(mixer->in_samples[i]);
        }
    }
    esp_gmf_oal_free(mixer->out_samples);
    esp_gmf_oal_free(mixer);
    return ESP_OK;
}

esp_err_t audio_mixer_add_stream(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    for (int i = 0; i < AUDIO_MAX_MIXER_NUM; i++) {
        if (mixer->bus[i] == NULL) {
            mixer->bus[i] = bus;
            mixer->in_samples[i] = (esp_ae_sample_t *)esp_gmf_oal_malloc_align(16, AUDIO_MIXER_SAMPLE_NUM_BYTES);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t audio_mixer_remove_stream(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    for (int i = 0; i < AUDIO_MAX_MIXER_NUM; i++) {
        if (mixer->bus[i] == bus) {
            mixer->bus[i] = NULL;
            esp_gmf_oal_free(mixer->in_samples[i]);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t audio_mixer_start(audio_mixer_handle_t handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    printf("audio_mixer_start event_group：%p\n", mixer);
    // esp_gmf_pipeline_run(mixer->pipe);
    xEventGroupSetBits(mixer->event_group, EVENT_GROUP_START);
    return ESP_OK;
}

esp_err_t audio_mixer_stop(audio_mixer_handle_t handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    mixer->process_running = false;
    // ret = esp_gmf_pipeline_stop(mixer->pipe);

    return ESP_OK;
}

