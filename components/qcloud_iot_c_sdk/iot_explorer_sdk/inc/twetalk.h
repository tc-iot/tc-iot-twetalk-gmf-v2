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

#define TWETALK_VERSION "1.1.4"

typedef enum {
    /** AI对话相关事件 */
    TWETALK_EVENT_BOT_START_SPEAKING = 0, /**< 机器人开始说话 */
    TWETALK_EVENT_BOT_STOP_SPEAKING  = 1, /**< 机器人停止说话 */
    TWETALK_EVENT_USR_START_SPEAKING = 2, /**< 用户开始说话 */
    TWETALK_EVENT_USR_STOP_SPEAKING  = 3, /**< 用户停止说话 */
    TWETALK_EVENT_BOT_TRANSCRIPTION  = 4, /**< 机器人字幕 */
    TWETALK_EVENT_USR_TRANSCRIPTION  = 5, /**< 用户字幕 */
    TWETALK_EVENT_METRICS_REPORT     = 6, /**< AI服务调用各模块耗时统计 */
    TWETALK_EVENT_IDLE_DETECTION     = 7, /**< 空闲检测, 目前是3次不回复就断开连接 */

    /** 设备呼叫小程序事件 */
    TWETALK_EVENT_RECV_USR_ANSWER = 10, /**< 设备呼叫小程序，小程序接听 */
    TWETALK_EVENT_RECV_USR_REJECT = 11, /**< 设备呼叫小程序，小程序拒绝 */
    TWETALK_EVENT_RECV_USR_HANGUP = 12, /**< 设备呼叫小程序，小程序主动挂断 */
    TWETALK_EVENT_RECV_USR_ERROR  = 13, /**< 设备呼叫小程序，发生错误❌ */

    /** 微信通话共同事件 */
    TWETALK_EVENT_RECV_START_CALL    = 20, /**< 服务端执行呼叫对端的动作 */
    TWETALK_EVENT_RECV_ENTER_CALLING = 21, /**< 服务端指示当前通话已建立，进入通话中状态 */
    TWETALK_EVENT_RECV_CALL_TIMEOUT  = 22, /**< 服务端呼叫超时 */
    TWETALK_EVENT_RECV_CALL_BUSY     = 23, /**< 服务端发起呼叫，但是对端正忙 */

    /** 小程序呼叫设备事件 */
    TWETALK_EVENT_RECV_USR_CALLING = 30, /**< 收到小程序呼叫 */
    TWETALK_EVENT_RECV_USR_CANCEL  = 31, /**< 当设备没接听此时小程序取消呼叫则会收到此消息 */
    TWETALK_EVENT_DEVICE_ANSWER    = 32, /**< 小程序呼叫设备，设备接听 */
    TWETALK_EVENT_DEVICE_REJECT    = 33, /**< 小程序呼叫设备，设备拒绝 */
    TWETALK_EVENT_DEVICE_HANGUP    = 34, /**< 小程序呼叫设备，设备主动挂断 */
    TWETALK_EVENT_RECV_DISCONNECT  = 35, /**< 收到断开连接 */
    TWETALK_EVENT_RECV_ERROR       = 36, /**< 接收错误 */
    TWETALK_EVENT_RECV_CLOSE       = 37, /**< WebSocket连接关闭 */

    /** TRTC相关事件，ws不关心 */
    TWETALK_EVENT_TRTC_REMOTE_USR_ENTER_ROOM = 50, /**< trtc远端用户进入房间 */
    TWETALK_EVENT_TRTC_REMOTE_USR_EXIT_ROOM  = 51, /**< trtc远端用户退出房间 */
    TWETALK_EVENT_TRTC_NOONE_READER_IN_ROOM  = 52, /**< trtc房间没有reader,意味着此时只有你一个在房间里 */

    /** 请求图片事件 */
    TWETALK_EVENT_REQUEST_IMAGE = 60, /**< 请求图片 */

    /** 最大值 */
    TWETALK_EVENT_MAX,
} TWeTalkEventType;

/**
 * @brief 事件消息
 *
 */
typedef union {
    struct {
        char transcription[512];
        char* long_transcription;  // 动态分配的长文本缓冲区
        int   is_dynamic;          // 标记是否使用了动态分配
    } BotTranscription;

    struct {
        char transcription[512];
        char* long_transcription;  // 动态分配的长文本缓冲区
        int   is_dynamic;          // 标记是否使用了动态分配
    } UsrTranscription;

    struct {
        char room_id[256];
        char caller_id[128];
    } RecvCalling;

    struct {
        char room_id[256];
    } RecvCancel;

    struct {
        int code;
    } RecvError;

    struct {
        char called[128];
        char openid[128];
    } UserAnswer;

    struct {
        char stream[256];
        char called[128];
        char openid[128];
    } UserHangup;

    struct {
        char called[128];
        char openid[128];
    } UserReject;

    struct {
        char called[128];
        char openid[128];
    } DeviceReject;

    struct {
        char called[128];
        char openid[128];
    } DeviceHangup;

    struct {
        char called[128]; /**< 被呼叫方的标识符，通常为设备ID或用户ID */
        char openid[128]; /**< 微信用户的唯一标识符，在同一个小程序下唯一 */
        int  code;        /**< 错误码，具体含义如下：
                            *   100:  - 微信client初始化失败，内部错误
                            *   101:  - 呼叫参数缺失
                            *   102:  - 设备没有注册
                            *   103:  - 设备票据失效
                            *   104:  - 设备与oppid不匹配
                            *   105:  - 房间号非法
                            *   106:  - 微信占线或其他错误
                            */
    } UserError;

    struct {
        char called[128];
        char openid[128];
    } ServerCallStart;

    struct {
        char called[128];
        char openid[128];
    } ServerCalling;

    struct {
        char called[128];
        char openid[128];
    } ServerCallTimeout;

    struct {
        char called[128];
        char openid[128];
    } ServerCallBusy;

    struct {
        char usr_id[128];
    } TrtcRemoteUsrEnterRoom;

    struct {
        char usr_id[128];
    } TrtcRemoteUsrExitRoom;

    struct {
        char transcription[512];
    } RequestImage;

    struct {
        char metrics[512];
    } Metrics;

} TWeTalkEventMsg;

/**
 * @brief TWeTalk事件邮件项
 *        用于在事件队列中传递事件信息
 */
typedef struct {
    TWeTalkEventType type;
    TWeTalkEventMsg  msg;
} TWeTalkEventMailItem;

/**
 * @brief 设备通讯录，当对话过程中说"给小明打电话"时，会从通讯录中查找对应的设备
 *        所以在通话☎️前需要更新次通讯录
 *
 */typedef struct {
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
    TWETALK_AUDIO_TYPE_AAC, /**< websocket暂不支持AAC,只在trtc接入时使用 */
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