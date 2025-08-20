/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2021 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file twetalk_app.c
 * @brief a simple sample for twetalk
 * @author hubertxxu (hubertxxu@tencent.com)
 * @version 1.0
 * @date 2021-05-31
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-31 <td>1.0     <td>hubertxxu   <td>first commit
 * <tr><td>2021-07-08 <td>1.1     <td>hubertxxu   <td>fix code standard of IotReturnCode and QcloudIotClient
 * <tr><td>2025-08-01 <td>1.2     <td>hubertxxu   <td>support twetalk websocket
 * </table>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_template_app.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "qcloud_iot_common.h"
#include "twetalk.h"
#include "utils_log.h"
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_gmf_afe.h"
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
#include "audio_processor.h"
#include "button_key.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_heap_caps.h"
#include "ota_downloader.h"
#include "pca9557.h"
#include "qcloud_iot_wifi_config.h"
#include "twetalk_app.h"

#define TAG "TWETALK"

#define DEFAULT_BUFFER_SIZE (4096)       // 4096
#define WAKEUP_TIMEOUT      (30 * 1000)  // 30s

static int sg_vad_start             = 0;
static int sg_wakeup_start          = 0;
static int sg_key_pressed           = 0;
static int sg_key_record_mode       = 0;
static int sg_ota_download_finished = 0;
static void *sg_twetalk_handle      = NULL;
static int sg_main_exit             = 0;
static int sg_twetalk_error         = 0;
static int sg_is_net_connected      = 0;
static int sg_check_twetalk_connect = 0;

static esp_gmf_oal_thread_t twetalk_thread;
static esp_gmf_oal_thread_t read_thread;
static esp_gmf_oal_thread_t ota_thread;

// ------------------------------------------------------------
// audio process
// ------------------------------------------------------------

const char *tone_uri[] = {
    "file://sdcard/System/connecting.aac",     "file://sdcard/System/connected.aac",
    "file://sdcard/System/hello.aac",          "file://sdcard/System/dong.aac",
    "file://sdcard/System/pair_network.aac",   "file://sdcard/System/clear_connect.aac",
    "file://sdcard/System/enter_key_mode.wav", "file://sdcard/System/exit_key_mode.wav",
};

static void audio_data_read_task(void *pv)
{
    uint8_t *data = esp_gmf_oal_calloc(1, DEFAULT_BUFFER_SIZE);

    int ret = 0;
    while (!sg_main_exit) {
        // TODO 发送本地音频
        ret = audio_recorder_read_data(data, DEFAULT_BUFFER_SIZE);
        if (sg_key_record_mode == 0 && sg_wakeup_start && sg_vad_start) {  // 唤醒对话模式
            tc_twetalk_ws_send_audio(sg_twetalk_handle, data, ret);
        } else if (sg_key_record_mode == 1 && sg_key_pressed) {  // 按键对话模式
            tc_twetalk_ws_send_audio(sg_twetalk_handle, data, ret);
        }
    }
    esp_gmf_oal_thread_delete(read_thread);
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static void recorder_event_callback_fn(void *event, void *ctx)
{
    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START:
            ESP_LOGI(TAG, "wakeup start");
            sg_wakeup_start          = 1;
            sg_check_twetalk_connect = 1;
            audio_prompt_play(tone_uri[LOCALPLAY_DONG]);
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

// ---------------------------audio process end ---------------------------------

/**
 * @brief MQTT event callback, @see MQTTEventHandleFun
 *
 * @param[in] client pointer to mqtt client
 * @param[in] handle_context context
 * @param[in] msg msg
 */
static void _mqtt_event_handler(void *client, void *handle_context, MQTTEventMsg *msg)
{
    MQTTMessage *mqtt_message = (MQTTMessage *)msg->msg;
    uintptr_t packet_id       = (uintptr_t)msg->msg;

    switch (msg->event_type) {
        case MQTT_EVENT_UNDEF:
            Log_i("undefined event occur.");
            break;

        case MQTT_EVENT_DISCONNECT:
            Log_i("MQTT disconnect.");
            break;

        case MQTT_EVENT_RECONNECT:
            Log_i("MQTT reconnect.");
            break;

        case MQTT_EVENT_PUBLISH_RECEIVED:
            Log_i("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                  mqtt_message->topic_len, STRING_PTR_PRINT_SANITY_CHECK(mqtt_message->topic_name),
                  mqtt_message->payload_len, STRING_PTR_PRINT_SANITY_CHECK((char *)mqtt_message->payload));
            break;
        case MQTT_EVENT_SUBSCRIBE_SUCCESS:
            Log_i("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBSCRIBE_TIMEOUT:
            Log_i("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBSCRIBE_NACK:
            Log_i("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBE_SUCCESS:
            Log_i("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBE_TIMEOUT:
            Log_i("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBE_NACK:
            Log_i("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_SUCCESS:
            Log_i("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_TIMEOUT:
            Log_i("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_NACK:
            Log_i("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;
        default:
            Log_i("Should NOT arrive here.");
            break;
    }
}

/**
 * @brief Setup MQTT construct parameters.
 *
 * @param[in,out] initParams @see MQTTInitParams
 * @param[in] device_info @see DeviceInfo
 */
static void _setup_connect_init_params(MQTTInitParams *init_params, DeviceInfo *device_info)
{
    init_params->device_info       = device_info;
    init_params->event_handle.h_fp = _mqtt_event_handler;
}

// ----------------------------------------------------------------------------
// OTA callback
// ----------------------------------------------------------------------------

int _on_download_finish(const char *version, size_t total_len)
{
    Log_d("download firmware: version[%s]|file_size[%d]", version, total_len);
    sg_ota_download_finished = 1;
    return 0;
}

const char *_get_firmware_version(void)
{
    return "esp32s3_v1.0.0";
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

/**
 * @brief 收到音频数据回调
 * @note 该回调在TWeTalk线程中执行，不要做耗时操作
 * @param recv_data 音频数据指针
 * @param recv_len 音频数据长度
 * @param context 用户上下文
 * @return int 0成功，非0失败
 */
static int _twetalk_recv_audio_cb(uint8_t *recv_data, int recv_len, void *context)
{
    if (sg_key_record_mode && sg_key_pressed) {
        return 0;  // key mode ignore
    }
    audio_playback_feed_data(recv_data, recv_len);
    return 0;
}

/**
 * @brief twetalk事件接收回调
 * @note 该回调在TWeTalk线程中执行，不要做耗时操作
 * @param[in] type 事件类型
 * @param[in] msg 事件消息
 * @param[in] context 用户上下文
 * @return 0成功，非0失败
 */
static int _twetalk_recv_event_cb(TWeTalkEventType type, TWeTalkEventMsg *msg, void *context)
{
    tc_twetalk_call_event_type_print(type);
    switch (type) {
        /**< 机器人开始讲话 */
        case TWETALK_EVENT_BOT_START_SPEAKING:
            Log_i("bot start speaking");
            break;

        /**< 机器人停止讲话 */
        case TWETALK_EVENT_BOT_STOP_SPEAKING:
            Log_i("bot stop speaking");
            break;

        /**< 机器人讲话字幕，可做UI显示，UTF-8编码 */
        case TWETALK_EVENT_BOT_TRANSCRIPTION: {
            Log_i("bot: %.*s", msg->BotTranscription.transcription.value_len,
                  msg->BotTranscription.transcription.value);
        } break;

        /**< 用户讲话字幕，可做UI显示，UTF-8编码 */
        case TWETALK_EVENT_USR_TRANSCRIPTION: {
            Log_i("usr: %.*s", msg->UsrTranscription.transcription.value_len,
                  msg->UsrTranscription.transcription.value);
        } break;

        /**< 收到小程序呼叫，可做UI显示，可以接听/挂断/不理会 */
        case TWETALK_EVENT_RECV_ROOMID: {
            Log_i("calling roomid: %.*s", msg->RecvCalling.room_id.value_len, msg->RecvCalling.room_id.value);
            // TODO 自定义是否接听
            tc_twetalk_call_response(sg_twetalk_handle, TWETALK_EVENT_DEVICE_ANSWER,
                                     &msg->RecvCalling.room_id);  // 接听
            // tc_twetalk_call_response(sg_twetalk_handle, TWETALK_EVENT_DEVICE_REJECTS, &msg->RecvCalling.room_id); //
            // 拒接
        } break;

        /**< 设备呼叫小程序，小程序接听 */
        case TWETALK_EVENT_RECV_USR_ANSWER: {
            Log_i("user answer called: %.*s openid: %.*s", msg->UserAnswer.called.value_len,
                  msg->UserAnswer.called.value, msg->UserAnswer.openid.value_len, msg->UserAnswer.openid.value);
        } break;

        /**< 小程序挂断 */
        case TWETALK_EVENT_RECV_USR_HANGUP: {
            Log_i("user hangup(%.*s) called: %.*s openid: %.*s", msg->UserHangup.stream.value_len,
                  msg->UserHangup.stream.value, msg->UserHangup.called.value_len, msg->UserHangup.called.value,
                  msg->UserHangup.openid.value_len, msg->UserHangup.openid.value);
        } break;

        /**< 设备呼叫小程序，发生错误❌ */
        case TWETALK_EVENT_RECV_USR_ERROR: {
            Log_w("code : %d", msg->UserError.code);
        } break;

        /**< 接收错误 */  // TODO : 错误处理
        case TWETALK_EVENT_RECV_ERROR: {
            Log_e("recv error: %d", msg->RecvError.code);
            sg_twetalk_error = msg->RecvError.code;
        } break;
        default:
            Log_w("unknown event type: %d", type);
            break;
    }
    return 0;
}

static int _ota_task_init(void *client)
{
    IotOtaInitParams params = {
        .on_download_finish   = _on_download_finish,
        .get_firmware_version = _get_firmware_version,
    };

    int rc = iot_ota_init(client, &params);
    if (rc) {
        return rc;
    }

    return esp_gmf_oal_thread_create(&ota_thread, "ota_thread", iot_ota_process, (void *)ota_thread, 4096, 2, false, 0);
}

static void twetalk_thread_entry(void *param)
{
    int rc;
    // init log level
    LogHandleFunc func = DEFAULT_LOG_HANDLE_FUNCS;
    utils_log_init(func, LOG_LEVEL_DEBUG, 2048);

    static DeviceInfo device_info = {
        .product_id    = CONFIG_QCLOUD_PRODUCT_ID,
        .device_name   = CONFIG_QCLOUD_DEVICE_NAME,
        .device_secret = CONFIG_QCLOUD_DEVICE_SECRET,
    };
    HAL_SetDevInfo(&device_info);
    HAL_Printf("\r\n\r\ncurrent version: %s\r\nbuild time : %s %s\r\ndevice_id : %s_%s \r\n\r\n",
               _get_firmware_version(), __DATE__, __TIME__, device_info.product_id, device_info.device_name);

    if (sg_is_net_connected == 0) {
        ESP_LOGW(TAG, "net not connect");
        IotWifiConfigParams params = {0};
        rc                         = iot_wifi_config(IOT_WIFI_BIND_TYPE_LLSYNC_BLE, &params, 5 * 60 * 1000);
        if (rc) {
            Log_e("wifi config failed: %d", rc);
        }
        audio_prompt_play(tone_uri[LOCALPLAY_CONNECTING]);
        HAL_SleepMs(5000);
        esp_restart();
    }

    // init connection
    MQTTInitParams init_params = DEFAULT_MQTT_INIT_PARAMS;
    _setup_connect_init_params(&init_params, &device_info);

    // create MQTT client and connect with server
    void *client = IOT_MQTT_Construct(&init_params);
    if (client) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("MQTT Construct failed!");
        return;
    }

    rc = usr_data_template_init(client);
    if (rc) {
        Log_e("usr data template init failed: %d", rc);
        IOT_MQTT_Destroy(&client);
        return;
    }

    TWeTalkWsInitParams twetalk_params      = DEFAULT_TWETALK_WS_INIT_PARAMS;
    twetalk_params.mqtt_client              = client;
    twetalk_params.recv_audio_cb            = _twetalk_recv_audio_cb;
    twetalk_params.recv_event_cb            = _twetalk_recv_event_cb;
    twetalk_params.context                  = NULL;
    twetalk_params.audio_type               = TWETALK_AUDIO_TYPE_OPUS;
    twetalk_params.push_recv_frame_interval = 50;
    // p2p player 测试小程序信息，自有小程序请替换为自己的小程序信息
    twetalk_params.wxa_appid   = "wx9e8fbc98ceac2628";
    twetalk_params.wxa_modelid = "DYEbVE9kfjAONqnWsOhXgw";

    sg_twetalk_handle = tc_twetalk_ws_init(&twetalk_params);
    if (sg_twetalk_handle == NULL) {
        Log_e("TWeTalk WebSocket init failed!");
        usr_data_template_deinit(client);
        IOT_MQTT_Destroy(&client);
        goto ret;
    }

    // init ota
    rc = _ota_task_init(client);
    if (rc) {
        Log_e("ota task init failed: %d", rc);
        usr_data_template_deinit(client);
        IOT_MQTT_Destroy(&client);
        goto ret;
    }

    // TODO 根据实际情况来更新物模型
    usr_report_battery(client, 100);
    usr_report_volume(client, 80);

    TWeCallOpenids openids[1];  // 如果有更多联系人则扩大数组，最多支持10个联系人
    memset(openids, 0, sizeof(openids));
    strcpy(openids[0].name, CONFIG_TWETALK_CALLING_NAME);
    strcpy(openids[0].open_id, CONFIG_TWETALK_CALLING_OPENID);
    tc_twetalk_call_sync_openids(sg_twetalk_handle, openids, 1);

    do {
        rc = IOT_MQTT_Yield(client, 200);
        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            HAL_SleepMs(1000);
            continue;
        } else if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
            Log_e("exit with error: %d", rc);
            break;
        }
        if (sg_ota_download_finished) {
            ESP_LOGI(TAG, "ota download finished");
            sg_main_exit = 1;
            break;  // 退出循环
        }
        if (sg_twetalk_error) {
            tc_twetalk_ws_disconnect(sg_twetalk_handle);
            sg_twetalk_error = 0;
        }
        if (sg_check_twetalk_connect) {
            sg_check_twetalk_connect = 0;
            // 长时间无对话后台会切掉websocket，所以如果断线需要重新连接
            if (twetalk_ws_is_connected(sg_twetalk_handle) != 1) {
                tc_twetalk_ws_reconnect(sg_twetalk_handle);
            }
        }
    } while (!sg_main_exit);
    rc |= tc_twetalk_ws_exit(sg_twetalk_handle);
    rc |= usr_data_template_deinit(client);
    iot_ota_deinit();
    rc |= IOT_MQTT_Destroy(&client);
ret:
    Log_w("twetalk thread exit with error: %d", rc);
    button_key_deinit();
    if (sg_ota_download_finished) {
        extern int HAL_OTA_SwitchToNewFirmware(void);
        HAL_OTA_SwitchToNewFirmware();
    }
    utils_log_deinit();
    esp_gmf_oal_thread_delete(twetalk_thread);
    return;
}

static void btn_event_process(struct ebtn_btn *btn, ebtn_evt_t evt)
{
    int cnt = ebtn_click_get_count(btn);
    if (evt == EBTN_EVT_ONCLICK) {
        ESP_LOGI(TAG, "EBTN_EVT_ONCLICK cnt %d", cnt);
        if (cnt == 2) {
            if (sg_key_record_mode == 0) {
                sg_key_record_mode = 1;
                ESP_LOGW(TAG, "enter key record mode");
                audio_prompt_play(tone_uri[LOCALPLAY_ENTER_KEY_MODE]);
            } else {
                audio_set_volume(0);  // defalt
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
            extern void erase_wifi_info(void);
            erase_wifi_info();
            audio_prompt_play(tone_uri[LOCALPLAY_CLEAR_NETWORK]);
            HAL_SleepMs(6000);
            esp_restart();
        }
    } else if (evt == EBTN_EVT_ONPRESS) {
        sg_key_pressed = 1;
        if (sg_key_record_mode) {
            sg_check_twetalk_connect = 1;
        }
    } else if (evt == EBTN_EVT_ONRELEASE) {
        sg_key_pressed = 0;
    }
}

int tc_twetalk_init(int is_net_connected)
{
#ifdef CONFIG_LCEDA_SZP_BOARD
    // enable audio pa
    pca9557_init();
    pa_en(1);
#endif /* CONFIG_LCEDA_SZP_BOARD */
    audio_pipe_open();
    if (is_net_connected == 0) {
        audio_prompt_play(tone_uri[LOCALPLAY_PAIR_NETWORK]);
    }
    sg_is_net_connected = is_net_connected;
    button_key_init(btn_event_process);
    esp_gmf_oal_thread_create(&read_thread, "audio_data_read_task", audio_data_read_task, (void *)NULL, 4096, 12, true,
                              1);
    return esp_gmf_oal_thread_create(&twetalk_thread, "twetalk_ws", twetalk_thread_entry, (void *)NULL, 20 * 1024, 5,
                                     false, 1);
}
