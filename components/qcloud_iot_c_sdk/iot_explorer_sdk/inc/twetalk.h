/**
 * @file tc_twetalk.h
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief
 * @version 0.1
 * @date 2025-07-25
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
#ifndef IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_H_
#define IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "qcloud_iot_error.h"
#include "utils_json.h"
#include "qcloud_iot_config.h"

typedef enum {
    /** AI对话相关事件 */
    TWETALK_EVENT_BOT_START_SPEAKING = 0, /**< 机器人开始说话 */
    TWETALK_EVENT_BOT_STOP_SPEAKING,      /**< 机器人停止说话 */
    TWETALK_EVENT_BOT_TRANSCRIPTION,      /**< 机器人字幕 */
    TWETALK_EVENT_USR_TRANSCRIPTION,      /**< 用户字幕 */

    /** 设备呼叫小程序事件 */
    TWETALK_EVENT_RECV_USR_ANSWER, /**< 设备呼叫小程序，小程序接听 */
    TWETALK_EVENT_RECV_USR_REJECT, /**< 设备呼叫小程序，小程序拒绝 */
    TWETALK_EVENT_RECV_USR_HANGUP, /**< 设备呼叫小程序，小程序主动挂断 */
    TWETALK_EVENT_RECV_USR_ERROR,  /**< 设备呼叫小程序，发生错误❌ */

    /** 小程序呼叫设备事件 */
    TWETALK_EVENT_RECV_ROOMID,   /**< 收到呼叫 */
    TWETALK_EVENT_DEVICE_ANSWER, /**< 小程序呼叫设备，设备接听 */
    TWETALK_EVENT_DEVICE_REJECT, /**< 小程序呼叫设备，设备拒绝 */
    TWETALK_EVENT_DEVICE_HANGUP, /**< 小程序呼叫设备，设备主动挂断 */

    TWETALK_EVENT_RECV_DISCONNECT, /**< 收到断开连接 */
    TWETALK_EVENT_RECV_ERROR,      /**< 接收错误 */
    TWETALK_EVENT_RECV_CLOSE,      /**< WebSocket连接关闭 */
    TWETALK_EVENT_BOT_MAX,
} TWeTalkEventType;

/**
 * @brief 事件消息
 *
 */
typedef union {
    struct {
        UtilsJsonValue transcription;
    } BotTranscription;

    struct {
        UtilsJsonValue transcription;
    } UsrTranscription;

    struct {
        UtilsJsonValue room_id;
    } RecvCalling;

    struct {
        int code;
    } RecvError;

    struct {
        UtilsJsonValue called;
        UtilsJsonValue openid;
    } UserAnswer;

    struct {
        UtilsJsonValue stream;
        UtilsJsonValue called;
        UtilsJsonValue openid;
    } UserHangup;

    struct {
        UtilsJsonValue called;
        UtilsJsonValue openid;
    } UserReject;

    struct {
        UtilsJsonValue called;
        UtilsJsonValue openid;
        int            code;
    } UserError;

} TWeTalkEventMsg;

/**
 * @brief 设备通讯录，当对话过程中说“给小明打电话”时，会从通讯录中查找对应的设备
 *        所以在通话☎️前需要更新次通讯录
 *
 */
typedef struct {
    char name[32];    /**< 用户昵称，如：妈妈、小明 */
    char open_id[64]; /**< 用户open_id，同一个用户在同一个小程序下的openid是唯一的 */
} TWeCallOpenids;

/**
 * @brief 音频回调，不要阻塞
 *
 */
typedef int (*twetalk_recv_audio_cb)(uint8_t *recv_data, int recv_len, void *context);

/**
 * @brief 事件回调，不要阻塞
 *
 */
typedef int (*twetalk_recv_event_cb)(TWeTalkEventType type, TWeTalkEventMsg *msg, void *context);

typedef enum {
    TWETALK_AUDIO_TYPE_PCM,
    TWETALK_AUDIO_TYPE_OPUS,
    TWETALK_AUDIO_TYPE_MAX,
} TWeTalkAudioType;

typedef struct {
    TWeTalkAudioType      audio_type;               /**< 音频类型，目前只支持opus */
    int                   frame_interval;           /**< 帧间隔，目前固定60ms */
    int                   push_recv_frame_interval; /**< 推送接收的音频数据间隔，目前固定60ms */
    void                 *mqtt_client;              /**< mqtt handle */
    twetalk_recv_audio_cb recv_audio_cb;            /**< 接收音频回调，不要阻塞 */
    twetalk_recv_event_cb recv_event_cb;            /**< 接收事件回调，不要阻塞 */
    void                 *context;                  /**< 透传给recv_audio_cb和recv_event_cb的参数 */
    int         ringbuffer_size; /**< 接收环形缓冲区大小，单位字节，传0表示不需要缓冲，直接传递给recv_audio_cb */
    IotBool     auto_reconnect;  /**< 是否自动重连 TODO ：待实现 */
    IotBool     is_encrypt;      /**< 是否加密传输 TODO : 待实现 */
    const char *wxa_appid;       /**< 微信通话的微信小程序appid，NULL表示不使用微信通话 */
    const char *wxa_modelid;     /**< 微信通话的微信小程序modelid， NULL表示不使用微信通话 */
} TWeTalkWsInitParams;

#ifdef AUTH_WITH_NO_TLS
#define DEFAULT_TWETALK_WS_INIT_PARAMS \
    {TWETALK_AUDIO_TYPE_OPUS, 60, 60, NULL, NULL, NULL, NULL, 90 * 180, 1, 0, NULL, NULL}
#else
#define DEFAULT_TWETALK_WS_INIT_PARAMS \
    {TWETALK_AUDIO_TYPE_OPUS, 60, 60, NULL, NULL, NULL, NULL, 90 * 180, 1, 1, NULL, NULL}
#endif 
/**
 * @brief 初始化AI对话
 *
 * @param params @see iv_ai_start_s
 * @return int:error code
 */
void *tc_twetalk_ws_init(TWeTalkWsInitParams *params);

/**
 * @brief 退出AI对话
 *
 * @return int:error code
 */
int tc_twetalk_ws_exit(void *handle);

/**
 * @brief 发送文本数据
 *
 * @param msg 文本数据
 * @param len 文本数据长度
 * @return int:error code
 */
int tc_twetalk_ws_send_msg(void *handle, char *msg, uint32_t len);

/**
 * @brief 发送音频数据
 *
 * @param audio 音频数据 len 音频数据长度
 * @return 0 for success, negative for error
 */
int tc_twetalk_ws_send_audio(void *handle, uint8_t *audio, uint32_t len);

/**
 * @brief 同步通讯录，当通讯录有变更时调用
 *
 * @param handle twetalk init时返回的句柄
 * @param openids 通讯录数组
 * @param count   通讯录数组长度，最大支持10组
 * @return 0 for success, negative for error
 */
int tc_twetalk_call_sync_openids(void *handle, TWeCallOpenids openids[], uint32_t count);

/**
 * @brief 设备对当前通话状态的回复，有如下几种情况：
 *  tc_twetalk_call_response(twetalk_handle, TWETALK_EVENT_DEVICE_ANSWER, roomid); // 小程序呼叫设备：设备应答
 *  tc_twetalk_call_response(twetalk_handle, TWETALK_EVENT_DEVICE_REJECT, roomid); // 小程序呼叫设备：设备拒绝
 *  tc_twetalk_call_response(twetalk_handle, TWETALK_EVENT_DEVICE_HANGUP, roomid); // 小程序呼叫设备：设备主动挂断
 *  tc_twetalk_call_response(twetalk_handle, TWETALK_EVENT_DEVICE_HANGUP, NULL);   // 设备呼叫小程序，设备主动挂断
 *
 * @param handle twetalk init时返回的句柄
 * @param type  @see TWeTalkEventType 只支持 TWETALK_EVENT_DEVICE_*
 * @param roomid 呼叫请求房间id，如果是设备主动挂断，则roomid为NULL
 * @return 0 for success, negative for error
 */
int tc_twetalk_call_response(void *handle, TWeTalkEventType type, UtilsJsonValue *roomid);

/**
 * @brief 断开ws连接，有以下情况：
 * 2. 当ws发生错误时，也就是收到 TWETALK_EVENT_RECV_ERROR 时需要主动断开，调用此函数
 *
 * @param handle twetalk init时返回的句柄
 * @return 0 for success, negative for error
 */
int tc_twetalk_ws_disconnect(void *handle);

/**
 * @brief 重新连接ws，当需要重启对话时调用此函数
 *
 * @param handle twetalk init时返回的句柄
 * @return 0 for success, negative for error
 */
int tc_twetalk_ws_reconnect(void *handle);

/**
 * @brief 获取ws连接状态
 *
 * @param handle twetalk init时返回的句柄
 * @return 1:connected, 0:not connected, negative for error
 */
int twetalk_ws_is_connected(void *handle);

/**
 * @brief 打印收到的事件类型
 *
 * @param type
 */
void tc_twetalk_call_event_type_print(TWeTalkEventType type);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_H_