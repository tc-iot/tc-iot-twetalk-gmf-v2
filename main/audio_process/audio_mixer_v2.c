#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_gmf_mixer.h"
#include "audio_mixer_v2.h"

#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_oal_mem.h"
#include "gmf_loader_setup_defaults.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"

static const char *TAG = "audio_mixer_v2";

typedef struct {
    audio_mixer_v2_cfg_t cfg;
    esp_gmf_task_handle_t task;
    esp_gmf_pool_handle_t pool;
    esp_gmf_pipeline_handle_t pipe;
    audio_volume_adjust_t volume_adjust[2];  // defalut ch2
} audio_mixer_v2_t;


static void adjust_pcm16_volume(void *data, int len, bool increase)
{
    if (!data || len <= 0 || (len % 2) != 0) {
        return;
    }
    int16_t *samples = (int16_t *)data;
    int sample_count = len / 2;
    if (increase) {
        for (int i = 0; i < sample_count; ++i) {
            int32_t v = (int32_t)samples[i] * 1.5;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            samples[i] = (int16_t)v;
        }
    } else {
        for (int i = 0; i < sample_count; ++i) {
            samples[i] = samples[i] / 1.5;
        }
    }
}

static int mixer_inport_acquire_read_slot0(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;

    esp_gmf_data_bus_block_t _blk = {0};
    memset(&_blk, 0, sizeof(esp_gmf_data_bus_block_t));
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;
    int ret = esp_gmf_db_acquire_read(mixer->cfg.bus[0], &_blk, wanted_size, block_ticks);
    blk->valid_size = _blk.valid_size;
    if (ret == ESP_GMF_IO_TIMEOUT) {
        return ESP_GMF_IO_TIMEOUT;
    } else if (ret < 0) {
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(mixer->cfg.bus[0], &_blk, block_ticks);

    if (mixer->volume_adjust[0] == AUDIO_VOLUME_INCREASE) {
        adjust_pcm16_volume(blk->buf, blk->valid_size, true);
    } else if (mixer->volume_adjust[0] == AUDIO_VOLUME_DECREASE) {
        adjust_pcm16_volume(blk->buf, blk->valid_size, false);
    }

    return wanted_size;
}

static int mixer_inport_release_read_slot0(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return blk->valid_size;
}

static int mixer_inport_acquire_read_slot1(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    esp_gmf_data_bus_block_t _blk = {0};
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;
    int ret = esp_gmf_db_acquire_read(mixer->cfg.bus[1], &_blk, wanted_size, block_ticks);
    blk->valid_size = _blk.valid_size;
    if (ret == ESP_GMF_IO_TIMEOUT) {
        return ESP_GMF_IO_TIMEOUT;
    } else if (ret < 0) {
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(mixer->cfg.bus[1], &_blk, block_ticks);

    if (mixer->volume_adjust[1] == AUDIO_VOLUME_INCREASE) {
        adjust_pcm16_volume(blk->buf, blk->valid_size, true);
    } else if (mixer->volume_adjust[1] == AUDIO_VOLUME_DECREASE) {
        adjust_pcm16_volume(blk->buf, blk->valid_size, false);
    }

    return wanted_size;
}

static int mixer_inport_release_read_slot1(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return blk->valid_size;
}

static int mixer_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{    
    return 0;
}

static int mixer_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    if (mixer->cfg.out_cb) {
        mixer->cfg.out_cb(mixer->cfg.ctx, blk->buf, blk->valid_size);
    }
    return 0;
}

esp_err_t audio_mixer_v2_new(audio_mixer_v2_cfg_t *cfg, audio_mixer_v2_handle_t *handle)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)esp_gmf_oal_calloc(1, sizeof(audio_mixer_v2_t));
    if (mixer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio mixer");
        return ESP_FAIL;
    }
    mixer->cfg = *cfg;

    esp_gmf_pool_init(&mixer->pool);

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

    esp_ae_mixer_cfg_t mixer_cfg = DEFAULT_ESP_GMF_MIXER_CONFIG();
    mixer_cfg.sample_rate = mixer->cfg.src_sample_rate;
    mixer_cfg.channel = mixer->cfg.src_channel;
    mixer_cfg.bits_per_sample = mixer->cfg.src_bits;
    static esp_ae_mixer_info_t src_info[] = {
        {.weight1 = 0, .weight2 = 0.8, .transit_time = 1000},
        {.weight1 = 0, .weight2 = 0.8, .transit_time = 1000}
    };
    mixer_cfg.src_num = 2;
    mixer_cfg.src_info = src_info;
    esp_gmf_mixer_init(&mixer_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    char *name[4] = {0};
    int name_count = 0;
    int inport_delay = 0;
    if (mixer->cfg.nb_streams == 1) {
        name[name_count++] = "aud_rate_cvt";
        name[name_count++] = "aud_ch_cvt";
        name[name_count++] = "aud_bit_cvt";
        inport_delay = 1000;
    } else {
        name[name_count++] = "aud_mixer";
        name[name_count++] = "aud_rate_cvt";
        name[name_count++] = "aud_ch_cvt";
        name[name_count++] = "aud_bit_cvt";
    }
    esp_gmf_pool_new_pipeline(mixer->pool, NULL, (const char **)name, name_count, NULL, &mixer->pipe);

    if (mixer->cfg.nb_streams == 2) {
        esp_gmf_element_handle_t mixer_hd = NULL;
        esp_gmf_pipeline_get_el_by_name(mixer->pipe, name[0], &mixer_hd);
        esp_gmf_mixer_set_mode(mixer_hd, 0, ESP_AE_MIXER_MODE_FADE_UPWARD);
        esp_gmf_mixer_set_mode(mixer_hd, 1, ESP_AE_MIXER_MODE_FADE_UPWARD);
        esp_gmf_mixer_set_audio_info(mixer_hd, mixer->cfg.src_sample_rate, mixer->cfg.src_bits, mixer->cfg.src_channel);
        printf("sample_rate: %d, channel: %d, bits: %d\n", mixer->cfg.src_sample_rate, mixer->cfg.src_channel, mixer->cfg.src_bits);
    }

    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_rate_cvt", &rate_cvt);
    esp_gmf_rate_cvt_set_dest_rate(rate_cvt, mixer->cfg.dst_sample_rate);

    esp_gmf_obj_handle_t ch_cvt = NULL; 
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_ch_cvt", &ch_cvt);
    esp_gmf_ch_cvt_set_dest_channel(ch_cvt, mixer->cfg.dst_channel);

    esp_gmf_obj_handle_t bit_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_bit_cvt", &bit_cvt);
    esp_gmf_bit_cvt_set_dest_bits(bit_cvt, mixer->cfg.dst_bits);

    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        mixer_inport_acquire_read_slot0,
        mixer_inport_release_read_slot0,
        NULL, (void *)mixer, 2048, 0);

    int ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, name[0], ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register input port");

    if (mixer->cfg.nb_streams == 2) {
        esp_gmf_port_handle_t inport2 = NEW_ESP_GMF_PORT_IN_BYTE(
            mixer_inport_acquire_read_slot1,
            mixer_inport_release_read_slot1,
            NULL, (void *)mixer, 2048, inport_delay);
        ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, name[0], ESP_GMF_IO_DIR_READER, inport2);
    }
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        mixer_outport_acquire_write,
        mixer_outport_release_write,
        NULL, (void *)mixer, 2048, portMAX_DELAY);

    ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, "aud_bit_cvt", ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register output port");

    esp_gmf_info_sound_t req_info = {
        .sample_rates = mixer->cfg.src_sample_rate,
        .channels = mixer->cfg.src_channel,
        .bits = mixer->cfg.src_bits,
    };
    esp_gmf_pipeline_report_info(mixer->pipe, ESP_GMF_INFO_SOUND, &req_info, sizeof(req_info));

    esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    task_cfg.ctx = NULL;
    task_cfg.cb = NULL;
    task_cfg.thread.core = 1;
    task_cfg.thread.prio = 10;
    task_cfg.name = "thread_mixer";
    esp_gmf_task_init(&task_cfg, &mixer->task);
    esp_gmf_pipeline_bind_task(mixer->pipe, mixer->task);
    esp_gmf_pipeline_loading_jobs(mixer->pipe);
    esp_gmf_task_set_timeout(mixer->task, 3000);

    *handle = mixer;
    return ESP_OK;
}

esp_err_t audio_mixer_v2_start(audio_mixer_v2_handle_t handle)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    esp_gmf_pipeline_run(mixer->pipe);
    return ESP_OK;
}

esp_err_t audio_mixer_v2_stop(audio_mixer_v2_handle_t handle)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    esp_gmf_pipeline_stop(mixer->pipe);
    return ESP_OK;
}

esp_err_t audio_mixer_v2_destroy(audio_mixer_v2_handle_t handle)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    esp_gmf_pipeline_destroy(mixer->pipe);
    return ESP_OK;
}

esp_err_t audio_mixer_v2_set_volume_adjust(audio_mixer_v2_handle_t handle, esp_gmf_db_handle_t bus, audio_volume_adjust_t adjust)
{
    audio_mixer_v2_t *mixer = (audio_mixer_v2_t *)handle;
    for (int i = 0; i < 2; i++) {   
        if (mixer->cfg.bus[i] == bus) {
            mixer->volume_adjust[i] = adjust;
            break;
        }
    }
    return ESP_OK;
}
