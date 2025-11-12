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
    TWETALK_EVENT_RECV_USR_CALLING, /**< 收到小程序呼叫 */
    TWETALK_EVENT_RECV_USR_CANCEL,  /**< 当设备没接听此时小程序取消呼叫则会收到此消息 */
    TWETALK_EVENT_DEVICE_ANSWER,    /**< 小程序呼叫设备，设备接听 */
    TWETALK_EVENT_DEVICE_REJECT,    /**< 小程序呼叫设备，设备拒绝 */
    TWETALK_EVENT_DEVICE_HANGUP,    /**< 小程序呼叫设备，设备主动挂断 */

    TWETALK_EVENT_RECV_DISCONNECT, /**< 收到断开连接 */
    TWETALK_EVENT_RECV_ERROR,      /**< 接收错误 */
    TWETALK_EVENT_RECV_CLOSE,      /**< WebSocket连接关闭 */

    /** TRTC相关事件，ws不关心 */
    TWETALK_EVENT_TRTC_REMOTE_USR_ENTER_ROOM, /**< trtc远端用户进入房间 */
    TWETALK_EVENT_TRTC_REMOTE_USR_EXIT_ROOM,  /**< trtc远端用户退出房间 */

    /** 请求图片事件 */
    TWETALK_EVENT_REQUEST_IMAGE,

    /** 最大值 */
    TWETALK_EVENT_MAX,
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
        UtilsJsonValue caller_id;
    } RecvCalling;

    struct {
        UtilsJsonValue room_id;
    } RecvCancel;

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

    struct {
        UtilsJsonValue usr_id;
    } TrtcRemoteUsrEnterRoom;

    struct {
        UtilsJsonValue usr_id;
    } TrtcRemoteUsrExitRoom;

    struct {
        UtilsJsonValue transcription;
    } RequestImage;

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
typedef int (*twetalk_recv_audio_cb)(uint8_t* recv_data, int recv_len, void* context);

/**
 * @brief 事件回调，不要阻塞
 *
 */
typedef int (*twetalk_recv_event_cb)(TWeTalkEventType type, TWeTalkEventMsg* msg, void* context);

typedef enum {
    TWETALK_AUDIO_TYPE_PCM,
    TWETALK_AUDIO_TYPE_OPUS,
    TWETALK_AUDIO_TYPE_MAX,
} TWeTalkAudioType;

typedef enum {
    TWETALK_LANGUAGE_TYPE_ZH,
    TWETALK_LANGUAGE_TYPE_EN,
    TWETALK_LANGUAGE_TYPE_MAX,
} TWeTalkLanguageType;

typedef struct {
    char             room_id[256];
    char             caller_id[128];
    TWeTalkEventType response;
} TWeTalkCallParams;

typedef struct {
    uint8_t* data;      /**< 图像数据指针 */
    uint32_t data_size; /**< 图像数据大小 */
    uint32_t width;     /**< 图像宽度 */
    uint32_t height;    /**< 图像高度 */
    char     format[8]; /**< 图像格式，如"jpeg"、"png" */
} TWeTalkImageFrame;

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_DEVICE_PROXY_INC_TWETALK_H_