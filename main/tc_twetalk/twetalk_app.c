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

#include "twetalk_app.h"

#define TAG "TWETALK"

#define DEFAULT_BUFFER_SIZE (4096)       // 4096
#define WAKEUP_TIMEOUT      (30 * 1000)  // 30s

#define DEVICEINFO_NVS_NAMESPACE "device_info"
#define TWETALK_LANGUAGE_NVS_KEY "language"

static int sg_vad_start             = 0;
static int sg_wakeup_start          = 0;
static int sg_key_pressed           = 0;
static int sg_key_record_mode       = 0;
static int sg_ota_download_finished = 0;
static void* sg_twetalk_handle      = NULL;
static int sg_main_exit             = 0;
static int sg_is_net_connected      = 0;
static void* sg_mail_queue          = NULL;

static esp_gmf_oal_thread_t twetalk_thread;
static esp_gmf_oal_thread_t read_thread;
static esp_gmf_oal_thread_t ota_thread;

extern int HAL_NVS_Write(const char* key, const uint8_t* value, uint32_t length);
extern int HAL_NVS_Read(const char* key, uint8_t* value, uint32_t* length);
extern int HAL_NVS_Erase(const char* key);

// ------------------------------------------------------------
// audio process
// ------------------------------------------------------------

const char* tone_uri[] = {
    "file://sdcard/System/connecting.aac",     "file://sdcard/System/connected.aac",
    "file://sdcard/System/hello.aac",          "file://sdcard/System/dong.aac",
    "file://sdcard/System/pair_network.aac",   "file://sdcard/System/clear_connect.aac",
    "file://sdcard/System/enter_key_mode.wav", "file://sdcard/System/exit_key_mode.wav",
};

static void audio_data_read_task(void* pv)
{
    uint8_t* data = esp_gmf_oal_calloc(1, DEFAULT_BUFFER_SIZE);

    int ret = 0;
    while (!sg_main_exit) {
        // TODO 发送本地音频
        ret = audio_recorder_read_data(data, DEFAULT_BUFFER_SIZE);
        if (!TWeTalk_WS_IsConnected(sg_twetalk_handle)) {
            // 断链不发送音频
            continue;
        }
        // ESP_LOGI("send","%d",ret);
        if (sg_key_record_mode == 0 && sg_wakeup_start) {  // 唤醒对话模式 && sg_vad_start
            TWeTalk_WS_SendAudio(sg_twetalk_handle, data, ret);
        } else if (sg_key_record_mode == 1 && sg_key_pressed) {  // 按键对话模式
            TWeTalk_WS_SendAudio(sg_twetalk_handle, data, ret);
        }
    }
    esp_gmf_oal_thread_delete(read_thread);
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static void recorder_event_callback_fn(void* event, void* ctx)
{
    esp_gmf_afe_evt_t* afe_evt = (esp_gmf_afe_evt_t*)event;
    switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START:
            ESP_LOGI(TAG, "wakeup start");
            sg_wakeup_start = 1;
            // 通过mailqueue发送检查连接事件
            if (sg_mail_queue) {
                TWeTalkAppMsg app_msg;
                app_msg.type = TWETALK_APP_EVENT_CHECK_CONNECT;
                TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
            }
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
static void _mqtt_event_handler(void* client, void* handle_context, MQTTEventMsg* msg)
{
    MQTTMessage* mqtt_message = (MQTTMessage*)msg->msg;
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
                  mqtt_message->payload_len, STRING_PTR_PRINT_SANITY_CHECK((char*)mqtt_message->payload));
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
static void _setup_connect_init_params(MQTTInitParams* init_params, DeviceInfo* device_info)
{
    init_params->device_info       = device_info;
    init_params->event_handle.h_fp = _mqtt_event_handler;
}

// ----------------------------------------------------------------------------
// OTA callback
// ----------------------------------------------------------------------------

int _on_download_finish(const char* version, size_t total_len)
{
    Log_d("download firmware: version[%s]|file_size[%d]", version, total_len);
    sg_ota_download_finished = 1;
    return 0;
}

const char* _get_firmware_version(void)
{
    return "esp32s3_v1.1.0";
}

static int get_twetalk_language(void)
{
    // 使用nvs实现
    nvs_handle_t nvs_handle;
    int32_t language = 0;  // 默认语言
    esp_err_t err;

    err = nvs_open(DEVICEINFO_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for reading language: %s", esp_err_to_name(err));
        return language;
    }

    err = nvs_get_i32(nvs_handle, TWETALK_LANGUAGE_NVS_KEY, &language);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read language from NVS: %s", esp_err_to_name(err));
        language = 0;  // 返回默认语言
    }

    nvs_close(nvs_handle);
    return language;
}

static int set_twetalk_language(int language)
{
    // 使用nvs实现
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(DEVICEINFO_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing language: %s", esp_err_to_name(err));
        return -1;
    }

    err = nvs_set_i32(nvs_handle, TWETALK_LANGUAGE_NVS_KEY, language);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write language to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit language to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return -1;
    }

    nvs_close(nvs_handle);
    return 0;
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
static int _twetalk_recv_audio_cb(uint8_t* recv_data, int recv_len, void* context)
{
    if (sg_key_record_mode && sg_key_pressed) {
        return 0;  // key mode ignore
    }
    // ESP_LOGI("recv","%d", recv_len);
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
static int _twetalk_recv_event_cb(TWeTalkEventType type, TWeTalkEventMsg* msg, void* context)
{
    TWeTalk_CallEventTypePrint(type);
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
        /**< caller id 主叫者id 即小程序的openId */
        case TWETALK_EVENT_RECV_USR_CALLING: {
            Log_i("calling roomid: %.*s caller id : %.*s", msg->RecvCalling.room_id.value_len,
                  msg->RecvCalling.room_id.value, msg->RecvCalling.caller_id.value_len,
                  msg->RecvCalling.caller_id.value);
            // 通过mailqueue发送事件到主线程处理
            TWeTalkAppMsg app_msg;
            app_msg.type      = TWETALK_APP_EVENT_WS_RECV_USR_CALLING;
            app_msg.event_msg = *msg;
            TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
        } break;
        /**< 收到小程序取消呼叫 */
        case TWETALK_EVENT_RECV_USR_CANCEL: {
            Log_i("usr cancel. roomid: %.*s", msg->RecvCancel.room_id.value_len, msg->RecvCancel.room_id.value);
            // 通过mailqueue发送事件到主线程处理
            TWeTalkAppMsg app_msg;
            app_msg.type      = TWETALK_APP_EVENT_WS_RECV_USR_CANCEL;
            app_msg.event_msg = *msg;
            TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
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
            // 通过mailqueue发送事件到主线程处理
            TWeTalkAppMsg app_msg;
            app_msg.type      = TWETALK_APP_EVENT_WS_RECV_USR_HANGUP;
            app_msg.event_msg = *msg;
            TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
        } break;

        /**< 设备呼叫小程序，发生错误❌ */
        case TWETALK_EVENT_RECV_USR_ERROR: {
            Log_w("code : %d", msg->UserError.code);
        } break;

        /**< 接收错误 */  // TODO : 错误处理
        case TWETALK_EVENT_RECV_ERROR: {
            Log_e("recv error: %d", msg->RecvError.code);
            // 通过mailqueue发送事件到主线程处理
            TWeTalkAppMsg app_msg;
            app_msg.type                     = TWETALK_APP_EVENT_WS_DISCONECT;
            app_msg.event_msg.RecvError.code = msg->RecvError.code;
            TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
        } break;
        default:
            Log_w("unknown event type: %d", type);
            break;
    }
    return 0;
}

static int _ota_task_init(void* client)
{
    IotOtaInitParams params = {
        .on_download_finish   = _on_download_finish,
        .get_firmware_version = _get_firmware_version,
    };

    int rc = iot_ota_init(client, &params);
    if (rc) {
        return rc;
    }

    return esp_gmf_oal_thread_create(&ota_thread, "ota_thread", iot_ota_process, (void*)ota_thread, 4096, 2, false, 0);
}

static void twetalk_thread_entry(void* param)
{
    int rc;
    // init log level
    LogHandleFunc func = DEFAULT_LOG_HANDLE_FUNCS;
    utils_log_init(func, LOG_LEVEL_DEBUG, 2048);
    Log_i("twetalk thread entry");
#if 1  // 测试用，正式使用请注释掉
    static DeviceInfo device_info = {
        .product_id    = "LTIHOHJW7F",
        .device_name   = "xiaoxing_04",
        .device_secret = "请从控制台获取",
    };
    // memset(&device_info, 0, sizeof(device_info));
    strncpy(device_info.device_version, _get_firmware_version(), sizeof(device_info.device_version) - 1);
#else
    static DeviceInfo device_info = {0};
#ifndef CONFIG_TWETALK_USE_DYNAMIC_REGISTER
    strncpy(device_info.product_id, CONFIG_QCLOUD_PRODUCT_ID, sizeof(CONFIG_QCLOUD_PRODUCT_ID) - 1);
    strncpy(device_info.device_name, CONFIG_QCLOUD_DEVICE_NAME, sizeof(CONFIG_QCLOUD_DEVICE_NAME) - 1);
    strncpy(device_info.device_secret, CONFIG_QCLOUD_DEVICE_SECRET, sizeof(CONFIG_QCLOUD_DEVICE_SECRET) - 1);
#else
    uint32_t read_len = sizeof(device_info);
    rc                = HAL_NVS_Read(DEVICEINFO_NVS_NAMESPACE, (uint8_t*)&device_info, &read_len);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read device info from NVS, use default");
        // TODO : 使用动态注册
        strncpy(device_info.product_id, CONFIG_QCLOUD_PRODUCT_ID, sizeof(CONFIG_QCLOUD_PRODUCT_ID) - 1);
        strncpy(device_info.product_secret, CONFIG_QCLOUD_PRODUCT_SECRET, sizeof(CONFIG_QCLOUD_PRODUCT_SECRET) - 1);
        // ! MAC地址做设备名称,
        // ! 注意⚠️：需要先在物联网开发平台预创建完设备，才可以在这里使用动态注册功能。
        uint8_t mac[6] = {0};
        HAL_GetMAC(mac, 6);
        HAL_Snprintf(device_info.device_name, sizeof(device_info.device_name), "%02X%02X%02X%02X%02X%02X", mac[0],
                     mac[1], mac[2], mac[3], mac[4], mac[5]);
        HAL_Snprintf(device_info.device_secret, sizeof(device_info.device_secret), "%s", "IOT_PSK");
    }
#endif  // CONFIG_TWETALK_USE_DYNAMIC_REGISTER
#endif
    TCI_HAL_SetDevInfo((uint8_t*)&device_info, sizeof(DeviceInfo));

    TCI_HAL_Printf("\r\n\r\n");
    TCI_HAL_Printf("==================================================\r\n");
    TCI_HAL_Printf("current version: %s\r\n", _get_firmware_version());
    TCI_HAL_Printf("build time     : %s %s\r\n", __DATE__, __TIME__);
    TCI_HAL_Printf("device_id      : %s_%s\r\n", device_info.product_id, device_info.device_name);
    TCI_HAL_Printf("==================================================\r\n");
    TCI_HAL_Printf("\r\n\r\n");

    if (sg_is_net_connected == 0) {
        ESP_LOGW(TAG, "net not connect");
        IotWifiConfigParams params = {0};
        rc                         = iot_wifi_config(TCIOT_WIFI_BIND_TYPE_LLSYNC_BLE, &params, 5 * 60 * 1000);
        if (rc) {
            Log_e("wifi config failed: %d", rc);
        }
        audio_prompt_play(tone_uri[LOCALPLAY_CONNECTING]);
        // TODO: 配网的时候可能执行了动态注册，所以这里再保存一次,保证下次可以正常读取到设备密钥
        TCI_HAL_GetDevInfo((uint8_t*)&device_info, sizeof(DeviceInfo));
        HAL_NVS_Write(DEVICEINFO_NVS_NAMESPACE, (const uint8_t*)&device_info, sizeof(device_info));
        TCI_HAL_SleepMs(5000);
        esp_restart();
    }

    // init connection
    MQTTInitParams init_params = DEFAULT_MQTT_INIT_PARAMS;
    _setup_connect_init_params(&init_params, &device_info);

    // create MQTT client and connect with server
    void* client = TCIOT_MQTT_Construct(&init_params);
    if (client) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("MQTT Construct failed!");
        goto ret;
    }

    rc = usr_data_template_init(client);
    if (rc) {
        Log_e("usr data template init failed: %d", rc);
        TCIOT_MQTT_Destroy(&client);
        goto ret;
    }

    TWeTalkWsInitParams twetalk_params      = DEFAULT_TWETALK_WS_INIT_PARAMS;
    twetalk_params.mqtt_client              = client;
    twetalk_params.recv_audio_cb            = _twetalk_recv_audio_cb;
    twetalk_params.recv_event_cb            = _twetalk_recv_event_cb;
    twetalk_params.context                  = NULL;
    twetalk_params.audio_type               = TWETALK_AUDIO_TYPE_OPUS;
    twetalk_params.push_recv_frame_interval = 40;

    twetalk_params.language_type = get_twetalk_language();  // 获取语言

    // twetalk 测试小程序信息，自有小程序请替换为自己的小程序信息
    twetalk_params.wxa_appid   = "wx7d65d685b7b00dae";
    twetalk_params.wxa_modelid = "0yQruCUX6y6MC7isot282g";

    sg_twetalk_handle = TWeTalk_WS_Init(&twetalk_params);
    if (sg_twetalk_handle == NULL) {
        Log_e("TWeTalk WebSocket init failed!");
        usr_data_template_deinit(client);
        TCIOT_MQTT_Destroy(&client);
        goto ret;
    }

    // init ota
    rc = _ota_task_init(client);
    if (rc) {
        Log_e("ota task init failed: %d", rc);
        TWeTalk_WS_Exit(sg_twetalk_handle);
        usr_data_template_deinit(client);
        TCIOT_MQTT_Destroy(&client);
        goto ret;
    }

    // TODO 根据实际情况来更新物模型
    usr_report_battery(client, 100);
    usr_report_volume(client, 80);

    TWeCallOpenids openids[1];  // 如果有更多联系人则扩大数组，最多支持10个联系人
    memset(openids, 0, sizeof(openids));
    strcpy(openids[0].name, CONFIG_TWETALK_CALLING_NAME);                             //
    strcpy(openids[0].open_id, CONFIG_TWETALK_CALLING_OPENID);  //
    TWeTalk_WS_CallSyncOpenids(sg_twetalk_handle, openids, 1);

    // 初始化mail queue（需要在按键初始化之前，因为按键回调可能会发送消息）
    sg_mail_queue = TCI_HAL_MailQueueInit(NULL, sizeof(TWeTalkAppMsg), 10);
    if (!sg_mail_queue) {
        Log_e("create mail queue failed");
        TWeTalk_WS_Exit(sg_twetalk_handle);
        usr_data_template_deinit(client);
        TCIOT_MQTT_Destroy(&client);
        goto ret;
    }

    TWeTalkAppMsg app_msg;
    size_t recv_len = 0;
    do {
        rc = TCI_HAL_MailQueueRecv(sg_mail_queue, &app_msg, &recv_len, 200);
        if (rc) {
            if (sg_main_exit == 1) {
                break;
            }
            continue;
        }
        Log_d("recv msg type: %d", app_msg.type);
        switch (app_msg.type) {
            case TWETALK_APP_EVENT_WS_DISCONECT: {
                TWeTalk_WS_Disconnect(sg_twetalk_handle);
            } break;

            case TWETALK_APP_EVENT_WS_RECV_USR_CALLING: {
                Log_i("calling roomid: %.*s caller id : %.*s", app_msg.event_msg.RecvCalling.room_id.value_len,
                      app_msg.event_msg.RecvCalling.room_id.value, app_msg.event_msg.RecvCalling.caller_id.value_len,
                      app_msg.event_msg.RecvCalling.caller_id.value);
                // TODO 自定义是否接听
                if (TWeTalk_WS_IsConnected(sg_twetalk_handle)) {
                    TWeTalk_WS_CallResponse(sg_twetalk_handle, TWETALK_EVENT_DEVICE_ANSWER,
                                            &app_msg.event_msg.RecvCalling.room_id);  // 接听
                    // TWeTalk_WS_CallResponse(sg_twetalk_handle, TWETALK_EVENT_DEVICE_REJECTS,
                    //                         &app_msg.event_msg.RecvCalling.room_id);  // 拒接
                } else {
                    // TODO ws断开 直接走特定ws连接打电话
                    TWeTalkCallParams call_params;
                    memset(&call_params, 0, sizeof(call_params));
                    call_params.response = TWETALK_EVENT_DEVICE_ANSWER;
                    strncpy(call_params.room_id, app_msg.event_msg.RecvCalling.room_id.value,
                            app_msg.event_msg.RecvCalling.room_id.value_len);
                    strncpy(call_params.caller_id, app_msg.event_msg.RecvCalling.caller_id.value,
                            app_msg.event_msg.RecvCalling.caller_id.value_len);
                    TWeTalk_WS_ReconnectWithCall(sg_twetalk_handle, &call_params);
                }
            } break;

            case TWETALK_APP_EVENT_WS_RECV_USR_CANCEL: {
                Log_i("handle user cancel event in main thread");
                // TODO: 添加取消呼叫的处理逻辑
            } break;

            case TWETALK_APP_EVENT_WS_RECV_USR_HANGUP: {
                Log_i("handle user hangup event in main thread");
                // TODO: 添加挂断的处理逻辑
            } break;

            case TWETALK_APP_EVENT_CHECK_CONNECT: {
                Log_d("check twetalk connect");
                // 长时间无对话后台会切掉websocket，所以如果断线需要重新连接
                if (TWeTalk_WS_IsConnected(sg_twetalk_handle) != 1) {
                    TWeTalk_WS_Reconnect(sg_twetalk_handle, 0);
                    TWeCallOpenids openids[1];  // 如果有更多联系人则扩大数组，最多支持10个联系人
                    memset(openids, 0, sizeof(openids));
                    strcpy(openids[0].name, CONFIG_TWETALK_CALLING_NAME);                             //
                    strcpy(openids[0].open_id, CONFIG_TWETALK_CALLING_OPENID);  //
                    TWeTalk_WS_CallSyncOpenids(sg_twetalk_handle, openids, 1);
                }
            } break;

            default:
                break;
        }
    } while (!sg_main_exit);
    rc |= TWeTalk_WS_Exit(sg_twetalk_handle);
    rc |= usr_data_template_deinit(client);
    iot_ota_deinit();
    rc |= TCIOT_MQTT_Destroy(&client);

    // 清理mailqueue
    if (sg_mail_queue) {
        TCI_HAL_MailQueueDeinit(sg_mail_queue);
        sg_mail_queue = NULL;
    }
ret:
    Log_w("twetalk thread exit with error: %d", rc);
    button_key_deinit();
    if (sg_ota_download_finished) {
        extern int HAL_OTA_SwitchToNewFirmware(void);
        HAL_OTA_SwitchToNewFirmware();
    }
    utils_log_deinit();
    esp_gmf_oal_thread_delete(twetalk_thread);
    vTaskDelete(NULL);  // 删除当前任务，防止FreeRTOS报错
    return;
}

static void btn_event_process(struct ebtn_btn* btn, ebtn_evt_t evt)
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
        if (cnt == 3) {
            int language = get_twetalk_language();
            if (language == 0) {
                language = 1;
            } else {
                language = 0;
            }
            ESP_LOGW(TAG, "set twetalk language %d", language);
            set_twetalk_language(language);
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
            TCI_HAL_SleepMs(6000);
            esp_restart();
        }
    } else if (evt == EBTN_EVT_ONPRESS) {
        sg_key_pressed = 1;
        if (sg_key_record_mode) {
            // 通过mailqueue发送检查连接事件
            if (sg_mail_queue) {
                TWeTalkAppMsg app_msg;
                app_msg.type = TWETALK_APP_EVENT_CHECK_CONNECT;
                TCI_HAL_MailQueueSend(sg_mail_queue, &app_msg, sizeof(app_msg), 0);
            }
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
    esp_gmf_oal_thread_create(&read_thread, "audio_data_read_task", audio_data_read_task, (void*)NULL, 4096, 12, true,
                              1);
    return esp_gmf_oal_thread_create(&twetalk_thread, "twetalk_ws", twetalk_thread_entry, (void*)NULL, 20 * 1024, 5,
                                     false, 1);
}
