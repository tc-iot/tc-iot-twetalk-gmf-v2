/**
 * @file twettalk_chat_app.c
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief
 * @version 0.1
 * @date 2025-07-17
 *
 * @copyright
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2021 - 2026 THL A29 Limited, a Tencent company.All rights reserved.
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "twetalk_chat_app.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_gmf_afe.h"
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
#include "audio_processor.h"
#include "button_key.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_heap_caps.h"
#include "lite-utils.h"
#include "pca9557.h"
#include "tc_iot_hal.h"
#include "tc_iot_log.h"
#include "virtual_ipc_ops.h"

#define TAG "TWETALK"

#define DEFAULT_BUFFER_SIZE (4096)  // 4096

#define JOY_ROLE "QQ_hard"  // QQ_hard QQ_soft pleasant_goat

#define WAKEUP_TIMEOUT (30 * 1000)  // 30s

typedef enum
{
    TWETALK_WS_INIT  = 0,       // 初始化
    TWETALK_WS_START = 1,       // 启动
    TWETALK_WS_EXIT,            // 退出
    TWETALK_WS_NEED_RECONNECT,  // 掉线重连
    TWETALK_WS_ERROR,           // 错误
    TWETALK_WS_RUNNING,         // 运行中
} twetalk_ws_state_e;

#if CONFIG_ENABLE_RECORDER_DEBUG
#define SAVE_RECORD_STREAM_TO_FILE
#endif  // CONFIG_ENABLE_RECORDER_DEBUG

static int sg_start_record = 0;

static AiWsInitParams sg_ws_init_params       = DEFAULT_AI_WS_INIT_PARAMS;
static twetalk_ws_state_e sg_twetalk_ws_state = 0;
static int sg_vad_start                       = 0;
static int sg_wakeup_start                    = 0;

static int sg_key_pressed     = 0;
static int sg_key_record_mode = 0;

#ifdef SAVE_RECORD_STREAM_TO_FILE

#define AEC_RECORD_TIME (60)

static void *record_fp = NULL;

static int create_fp_record_file(int cnt)
{
    char pathname[64];
    if (record_fp) {
        fclose(record_fp);
    }
    HAL_Snprintf(pathname, 64, "/sdcard/0723_record.pcm");
    ESP_LOGI(TAG, "create file:%s", pathname);
    record_fp = fopen(pathname, "wb");
    if (record_fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return -1;
    }
    return 0;
}

static int close_fp_record_file(int cnt)
{
    ESP_LOGW(TAG, "close file:record_%d.pcm", cnt);
    if (record_fp != NULL) {
        fclose(record_fp);
        record_fp = NULL;
    }
    return 0;
}

static int save_record_stream(TCIpcFrame *data)
{
    if (!record_fp) {
        return 0;
    }
    if ((esp_timer_get_time()) / 1000000 > AEC_RECORD_TIME) {
        close_fp_record_file(0);
        return 0;
    }

    fwrite(data->data, 1, data->size, record_fp);
    return 0;
}

#endif  //  SAVE_RECORD_STREAM_TO_FILE

// 开始播放
int pipline_play_start(TCIVStreamType type)
{
    ESP_LOGI(TAG, "start play");
    return 0;
}

// 停止播放
int pipline_play_stop(TCIVStreamType type)
{
    ESP_LOGI(TAG, "stop play");
    return 0;
}

// 开始录音
int pipline_record_start(TCIVStreamType type)
{
    sg_start_record = 1;
    return 0;
}

// 停止录音
int pipline_record_stop(TCIVStreamType type)
{
    sg_start_record = 0;
    return 0;
}

// 播放流
int pipline_play_stream(TCIpcFrame *data)
{
#if 0
    // ESP_LOGD(TAG, "play stream %d", data->size);
    static int idle_cnt   = 0;
    static int cnt        = 0;
    static bool idle_flag = true;

    if ((data->size == 279 || data->size == 280) && (idle_flag == false)) {
        idle_cnt++;
        // 连续50帧没有检测到说话，则认为大模型没有说话，关闭唤醒，保证连续对话
        if (idle_cnt > 30) {
            ESP_LOGW(TAG, "current is idle");
            idle_cnt  = 0;
            idle_flag = true;
        }
    } else if (data->size != 279 && data->size != 280 && idle_flag == true) {
        idle_cnt  = 0;
        idle_flag = false;
        ESP_LOGW(TAG, "speaking...");
    } else if (data->size != 279 && data->size != 280) {
        idle_cnt  = 0;
        idle_flag = false;
    }

    if (idle_flag == false) {
        ESP_LOGD(TAG, "play %d bytes", data->size);
        audio_playback_feed_data(data->data, data->size);
    }
#endif
    if (sg_key_record_mode == 1 && sg_key_pressed == 1) {
        return 0;
    }
    audio_playback_feed_data(data->data, data->size);
    return 0;
}

// 推送本地录音数据
int pipline_push_record_data(uint8_t *voiceData, uint32_t len)
{
    TCIpcFrame frame;
    frame.stream_type = TCIV_STREAM_TYPE_AUDIO;
    frame.data        = voiceData;
    frame.size        = len;
    frame.pts         = HAL_GetTimeMs();
#ifdef SAVE_RECORD_STREAM_TO_FILE
    save_record_stream(&frame);
#endif /* SAVE_RECORD_STREAM_TO_FILE */

    // 按键模式下，推送本地录音数据
    if (sg_key_record_mode == 1) {
        if (sg_key_pressed == 1 && sg_twetalk_ws_state == TWETALK_WS_RUNNING) {
            return qcloud_push_record_stream(&frame);
        } else {
            return 0;
        }
    }

    if (sg_wakeup_start == 0 || sg_vad_start == 0 || sg_twetalk_ws_state != TWETALK_WS_RUNNING) {
        return 0;
    }

    ESP_LOGD(TAG, "record %d bytes", len);
    return qcloud_push_record_stream(&frame);
}

// --------------------------------------------------------------
// music hal
// ---------------------------------------------------------------

int HAL_Music_Play(void **player, SongInfo *song_info)
{
    return 0;
}

int HAL_Music_Stop(void **player)
{
    return 0;
}

int HAL_Music_PlayPause(void *qq_player, uint8_t playPause)
{
    return 0;
}

int HAL_Music_SetVolume(void *qq_player, int volume)
{
    return 0;
}

int HAL_Music_GetVolume(void *qq_player)
{
    return 0;
}

int HAL_Music_PlayEndCheck(void *qq_player)
{
    return 0;
}

int HAL_Music_GetPlayPosition(void *qq_player)
{
    return 0;
}

int HAL_Music_SetPlayPosition(void *qq_player, int position)
{
    return 0;
}

// -------------------------------------------------------------- //

// --------------------------------------------------------------- //
static uint32_t sg_wakeup_countdown = WAKEUP_TIMEOUT;

static void reload_wakeup_count(void)
{
    sg_wakeup_countdown = WAKEUP_TIMEOUT;
}

static void audio_data_read_task(void *pv)
{
    uint8_t *data = esp_gmf_oal_calloc(1, DEFAULT_BUFFER_SIZE);

    int ret = 0;
    while (true) {
#if defined CONFIG_KEY_PRESS_DIALOG_MODE
        xEventGroupWaitBits(coze_chat.data_evt_group, BUTTON_REC_READING, pdFALSE, pdFALSE, portMAX_DELAY);
        ret = audio_recorder_read_data(data, DEFAULT_BUFFER_SIZE);
        if (ret > 0) {
            esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
        }

#elif defined CONFIG_VOICE_WAKEUP_MODE
        ret = audio_recorder_read_data(data, DEFAULT_BUFFER_SIZE);
        pipline_push_record_data(data, ret);
        // if (coze_chat.wakeuped) {
        //     esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
        // }
#else
        ret = audio_recorder_read_data(data, DEFAULT_BUFFER_SIZE);
        // esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
    }
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static void recorder_event_callback_fn(void *event, void *ctx)
{
    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START:
            ESP_LOGI(TAG, "wakeup start");
            sg_wakeup_start = 1;
            reload_wakeup_count();
            audio_prompt_play(tone_uri[LOCALPLAY_DONG]);
            if (sg_twetalk_ws_state == TWETALK_WS_NEED_RECONNECT) {
                sg_twetalk_ws_state = TWETALK_WS_START;
            }
            break;
        case ESP_GMF_AFE_EVT_WAKEUP_END:
            ESP_LOGI(TAG, "wakeup end");
            break;
        case ESP_GMF_AFE_EVT_VAD_START:
            ESP_LOGI(TAG, "vad start");
            sg_vad_start = 1;
            break;
        case ESP_GMF_AFE_EVT_VAD_END:
            ESP_LOGI(TAG, "vad end");
            sg_vad_start = 0;
            break;
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
            ESP_LOGI(TAG, "vcmd detect timeout");
            break;
        default: {
            // TODO: vcmd detected
            // esp_gmf_afe_vcmd_info_t *info = event->event_data;
            // ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s", sevent->type, info->phrase_id, info->prob,
            // info->str);
        }
    }
}
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */

static void audio_pipe_open(void)
{
    audio_manager_init();
    // audio_mixer_open();

#if CONFIG_KEY_PRESS_DIALOG_MODE
    audio_recorder_open(NULL, NULL);
#else
    audio_prompt_open();
    audio_recorder_open(recorder_event_callback_fn, NULL);
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
    audio_playback_open();
    audio_playback_run();
}

static void btn_event_process(struct ebtn_btn *btn, ebtn_evt_t evt)
{
    int cnt = ebtn_click_get_count(btn);
    if (evt == EBTN_EVT_ONCLICK) {
        ESP_LOGI(TAG, "EBTN_EVT_ONCLICK cnt %d", cnt);
        if (cnt == 2) {
            if (sg_key_record_mode == 0) {
                sg_key_record_mode = 1;
                audio_set_volume(100);
                ESP_LOGW(TAG, "enter key record mode");
                audio_prompt_play(tone_uri[LOCALPLAY_ENTER_KEY_MODE]);
            } else {
                audio_set_volume(0); //defalt
                ESP_LOGW(TAG, "exit key record mode");
                sg_key_record_mode = 0;
                audio_prompt_play(tone_uri[LOCALPLAY_EXIT_KEY_MODE]);
            }
        }
        if (cnt == 5) {
            esp_restart();
        }
    } else if (evt == EBTN_EVT_KEEPALIVE) {
        cnt = ebtn_keepalive_get_count(btn);
        ESP_LOGI(TAG, "EBTN_EVT_KEEPALIVE cnt %d", cnt);
        // 长按5s清除wifi信息
        if (cnt == 10 && sg_key_record_mode == 0) {
            ESP_LOGW(TAG, "clear wifi info");
            HAL_FileRemove("/sdcard/wifi_info.json");
            audio_prompt_play(tone_uri[LOCALPLAY_CLEAR_NETWORK]);
            HAL_SleepMs(6000);
            esp_restart();
        }
    } else if (evt == EBTN_EVT_ONPRESS) {
        sg_key_pressed = 1;
    } else if (evt == EBTN_EVT_ONRELEASE) {
        sg_key_pressed = 0;
    }
}

static void twetalk_ws_state_process(twetalk_ws_state_e *state)
{
    int rc = 0;
    switch (*state) {
        case TWETALK_WS_INIT:
            break;
        case TWETALK_WS_START:
            ESP_LOGI(TAG, "TWETALK_WS_START");
            rc = iv_avt_ai_ws_init(&sg_ws_init_params);
            if (rc) {
                ESP_LOGE(TAG, "iv_avt_ai_ws_init failed (%d)", rc);
                *state = TWETALK_WS_NEED_RECONNECT;
            } else {
                *state = TWETALK_WS_RUNNING;
            }
            break;
        case TWETALK_WS_ERROR:
            ESP_LOGE(TAG, "TWETALK_WS_ERROR");
            iv_avt_ai_ws_exit();
            *state = TWETALK_WS_NEED_RECONNECT;
            break;
        default:
            break;
    }
}

static void twetalk_ws_thread_entry(void *arg)
{
    int loop = 0;
    ESP_LOGI(TAG, "twetalk_ws_thread_entry started");
    while (1) {
        ebtn_process(HAL_GetTicksTimeMs());
        HAL_SleepMs(10);
        twetalk_ws_state_process(&sg_twetalk_ws_state);
        if (loop++ % 2000 == 0) {
            ESP_LOGI(TAG, "MEM Total:%d Bytes, Inter:%d Bytes, Dram:%d Bytes, Dram largest free:%zuBytes\r\n",
                     (int)esp_get_free_heap_size(), (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        }
        if (sg_wakeup_countdown) {
            sg_wakeup_countdown -= 10;
            if (sg_wakeup_countdown == 0) {
                sg_wakeup_start = 0;
                ESP_LOGW(TAG, "Need to wakeup again!");
            }
        }
    }
}

static int _ws_recv_cb(uint8_t *recv_data, int recv_len, int is_text)
{
    TCIpcFrame recv_frame;
    recv_frame.stream_type = TCIV_STREAM_TYPE_AUDIO;
    // Log_d("play:%d", recv_len);
    if (!is_text) {
        // 音频流直接播放
        recv_frame.data = recv_data;
        recv_frame.size = recv_len;
        ESP_LOGI("recv<-<", "%d", recv_len);
        reload_wakeup_count();
        extern int qcloud_push_play_stream_to_queue(TCIpcFrame * stream_data);
        qcloud_push_play_stream_to_queue(&recv_frame);
    } else {
        // 字幕  t: {"state":"sentence","text":"好的，为您播放冯提莫,既视感","type":"assistant"}
        Log_d("text:%s", recv_data);
        char *text = LITE_json_value_of("text", recv_data);
        char *type = LITE_json_value_of("type", recv_data);
        if (type && text) {
            Log_d("[%s]\t:[%s]", type, text);

            if (strstr(type, "assistant")) {
            } else if (strstr(type, "user")) {
            }

            if (strcmp(type, "system") == 0) {
                if (strcmp(text, "idle") == 0) {
                } else if (strcmp(text, "speaking") == 0) {
                } else if (strstr(text, "recv error")) {
                    Log_e("recv error\n");
                    sg_twetalk_ws_state = TWETALK_WS_ERROR;
                }
            }
        }
        HAL_Free(text);
        HAL_Free(type);
    }
    return 0;
}

static int twetalk_ws_init(DeviceInfo *dev_info)
{
    // websocket 通道
    sg_ws_init_params.ws_url =
        "ws://stress-test.tencentiotcloud.com/ws?role_id=" JOY_ROLE;  //  ws://iot-twetalk-webrtc-test.tencentiotcloud.com/ws?role_id= ws://stress-test.tencentiotcloud.com/ws?role_id=
    sg_ws_init_params.port        = 80;
    sg_ws_init_params.product_id  = dev_info->product_id;
    sg_ws_init_params.device_name = dev_info->device_name;
    sg_ws_init_params.recv_cb     = _ws_recv_cb;
    sg_twetalk_ws_state           = TWETALK_WS_INIT;
    return 0;
}

int twetalk_chat_audio_init(DeviceInfo *dev_info)
{
    static esp_gmf_oal_thread_t read_thread;
    static esp_gmf_oal_thread_t daemon_thread;

    TCIPIpcPlay ipc_play     = {.play_queue_size = 10,
                                .start_play      = pipline_play_start,
                                .stop_play       = pipline_play_stop,
                                .play_stream     = pipline_play_stream};
    TCIVIpcRecord ipc_record = {
        .record_queue_size = 10, .start_record = pipline_record_start, .stop_record = pipline_record_stop};

    qcloud_virtual_ipc_init(&ipc_play, &ipc_record);

#ifdef CONFIG_LCEDA_SZP_BOARD
    // enable audio pa
    pca9557_init();
    pa_en(1);
#endif /* CONFIG_LCEDA_SZP_BOARD */
    audio_pipe_open();
    button_key_init(btn_event_process);
    twetalk_ws_init(dev_info);
    esp_gmf_oal_thread_create(&read_thread, "audio_data_read_task", audio_data_read_task, (void *)NULL, 4096, 12, true,
                              1);
    esp_gmf_oal_thread_create(&daemon_thread, "twetalk_ws", twetalk_ws_thread_entry, (void *)NULL, 20 * 1024, 5, true,
                              1);
#ifdef SAVE_RECORD_STREAM_TO_FILE
    create_fp_record_file(1);
#endif /* SAVE_RECORD_STREAM_TO_FILE */
    return 0;
}

int twetalk_ws_start(int enable)
{
    sg_twetalk_ws_state = enable;
    IOT_Log_Set_Level((TC_LOG_LEVEL)eLOG_INFO);
    return 0;
}