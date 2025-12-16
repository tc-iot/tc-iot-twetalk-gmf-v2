/**
 * @file twetalk_ws.h
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
#ifndef IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_WS_H_
#define IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_WS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "twetalk.h"

// ------------------------------------------------------------------------------------------
// twetalk over websocket
// ------------------------------------------------------------------------------------------

typedef struct {
    char* host;
    int   port;
} TWeTalkWsServerInfo;

typedef struct {
    TWeTalkAudioType      audio_type;               /**< 音频类型，支持Opus和PCM */
    TWeTalkLanguageType   language_type;            /**< 语言类型，目前只支持中文和英文 */
    int                   frame_interval;           /**< 帧间隔，目前固定60ms */
    int                   push_recv_frame_interval; /**< 推送接收的音频数据间隔，目前固定60ms */
    void*                 mqtt_client;              /**< mqtt handle */
    TWeTalkWsServerInfo   ws_server;                /**< websocket server info */
    TWeTalkWsServerInfo   ws_wxa_server;            /**< 微信通话websocket server info */
    twetalk_recv_audio_cb recv_audio_cb;            /**< 接收音频回调，不要阻塞 */
    twetalk_recv_event_cb recv_event_cb;            /**< 接收事件回调，不要阻塞 */
    void*                 context;                  /**< 透传给recv_audio_cb和recv_event_cb的参数 */
    int         ringbuffer_size; /**< 接收环形缓冲区大小，单位字节，传0表示不需要缓冲，直接传递给recv_audio_cb */
    IotBool     use_vl;          /**< 是否多模态接入，需要在控制台购买多模态对应的license才能使用 */
    IotBool     auto_reconnect;  /**< 是否自动重连 TODO ：待实现 */
    IotBool     is_encrypt;      /**< 是否加密传输 TODO : 待实现 */
    const char* wxa_appid;       /**< 微信通话的微信小程序appid，NULL表示不使用微信通话 */
    const char* wxa_modelid;     /**< 微信通话的微信小程序modelid， NULL表示不使用微信通话 */
} TWeTalkWsInitParams;

#ifdef ENABLE_AUTH_NO_TLS
#define DEFAULT_TWETALK_WS_INIT_PARAMS \
    {TWETALK_AUDIO_TYPE_OPUS,          \
     TWETALK_LANGUAGE_TYPE_ZH,         \
     60,                               \
     60,                               \
     NULL,                             \
     {NULL, 0},                        \
     {NULL, 0},                        \
     NULL,                             \
     NULL,                             \
     NULL,                             \
     90 * 180,                         \
     0,                                \
     1,                                \
     0,                                \
     NULL,                             \
     NULL}
#else
#define DEFAULT_TWETALK_WS_INIT_PARAMS \
    {TWETALK_AUDIO_TYPE_OPUS,          \
     TWETALK_LANGUAGE_TYPE_ZH,         \
     60,                               \
     60,                               \
     NULL,                             \
     {NULL, 0},                        \
     {NULL, 0},                        \
     NULL,                             \
     NULL,                             \
     NULL,                             \
     90 * 180,                         \
     0,                                \
     1,                                \
     1,                                \
     NULL,                             \
     NULL}
#endif

/**
 * @brief 初始化AI对话
 *
 * @param params @see iv_ai_start_s
 * @return int:error code
 */
void* TWeTalk_WS_Init(TWeTalkWsInitParams* params);

/**
 * @brief 退出AI对话
 *
 * @return int:error code
 */
int TWeTalk_WS_Exit(void* handle);

/**
 * @brief 发送文本数据
 *
 * @param msg 文本数据
 * @param len 文本数据长度
 * @return int:error code
 */
int TWeTalk_WS_SendMsg(void* handle, char* msg, uint32_t len);

/**
 * @brief 发送音频数据
 *
 * @param audio 音频数据 len 音频数据长度
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_SendAudio(void* handle, uint8_t* audio, uint32_t len);

/**
 * @brief 发送图片数据
 *
 * @param handle twetalk init时返回的句柄
 * @param frame 图片帧数据 @see TWeTalkImageFrame
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_SendImage(void* handle, TWeTalkImageFrame* frame);

/**
 * @brief 同步通讯录，当通讯录有变更时调用
 *
 * @param handle twetalk init时返回的句柄
 * @param openids 通讯录数组
 * @param count   通讯录数组长度，最大支持10组
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_CallSyncOpenids(void* handle, TWeCallOpenids openids[], uint32_t count);

/**
 * @brief 设备对当前通话状态的回复，有如下几种情况：
 *  TWeTalk_WS_CallResponse(twetalk_handle, TWETALK_EVENT_DEVICE_ANSWER, roomid); // 小程序呼叫设备：设备应答
 *  TWeTalk_WS_CallResponse(twetalk_handle, TWETALK_EVENT_DEVICE_REJECT, roomid); // 小程序呼叫设备：设备拒绝
 *  TWeTalk_WS_CallResponse(twetalk_handle, TWETALK_EVENT_DEVICE_HANGUP, roomid); // 小程序呼叫设备：设备主动挂断
 *  TWeTalk_WS_CallResponse(twetalk_handle, TWETALK_EVENT_DEVICE_HANGUP, NULL);   // 设备呼叫小程序，设备主动挂断
 *
 * @param handle twetalk init时返回的句柄
 * @param type  @see TWeTalkEventType 只支持 TWETALK_EVENT_DEVICE_*
 * @param roomid 呼叫请求房间id，如果是设备主动挂断，则roomid为NULL
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_CallResponse(void* handle, TWeTalkEventType type, UtilsJsonValue* roomid);

/**
 * @brief 断开ws连接，有以下情况：
 * 2. 当ws发生错误时，也就是收到 TWETALK_EVENT_RECV_ERROR 时需要主动断开，调用此函数
 *
 * @param handle twetalk init时返回的句柄
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_Disconnect(void* handle);

/**
 * @brief 重新连接ws，当需要重启对话时调用此函数
 * @note 不要在回调函数中调用此函数，此函数会阻塞
 * @param handle twetalk init时返回的句柄
 * @param type 语言类型 当前支持中英文
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_Reconnect(void* handle, TWeTalkLanguageType type);

/**
 * @brief 重新连接ws，当需要直接通话时调用此函数
 * @note 不要在回调函数中调用此函数，此函数会阻塞
 * @param handle twetalk init时返回的句柄
 * @param params 呼叫参数
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_ReconnectWithCall(void* handle, TWeTalkCallParams* params);

/**
 * @brief 获取ws连接状态
 *
 * @param handle twetalk init时返回的句柄
 * @return 1:connected, 0:not connected, negative for error
 */
int TWeTalk_WS_IsConnected(void* handle);

/**
 * @brief 打印收到的事件类型
 *
 * @param type
 */
void TWeTalk_CallEventTypePrint(TWeTalkEventType type);

/**
 * @brief 从mail queue中获取事件（当初始化时未设置recv_event_cb回调时使用）
 *
 * @param handle twetalk init时返回的句柄
 * @param type 输出参数，事件类型
 * @param msg 输出参数，事件消息
 * @param timeout_ms 超时时间，单位毫秒，0表示不等待，0xFFFFFFFF表示永久等待
 * @return 0 for success, negative for error, QCLOUD_ERR_TWETALK_TIMEOUT表示超时
 */
int TWeTalk_WS_GetEvent(void* handle, TWeTalkEventType* type, TWeTalkEventMsg* msg, uint32_t timeout_ms);

/**
 * @brief 发送文本到LLM
 *
 * @param handle twetalk init时返回的句柄
 * @param msg 文本消息
 * @param msg_len 文本消息长度
 * @return 0 for success, negative for error
 */
int TWeTalk_WS_SendTextToLLM(void* handle, const char* msg, size_t msg_len);

// --------------------------------------- twetalk over websocket end ------------------------------

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_WS_H_