/**
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2022 THL A29 Limited, a Tencent company.All rights reserved.
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
 * @file qcloud_iot_ws_message.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-12-09
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-12-09 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WS_MESSAGE_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WS_MESSAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "qcloud_iot_common.h"

typedef enum {
    IOT_WS_MESSAGE_PUBLISH_TYPE_ACTIVE_PUSH,
    IOT_WS_MESSAGE_PUBLISH_TYPE_REFRESH_TOKEN,
} IotWsMessagePublishType;

/**
 * @brief Callback of web socket message.
 *
 */
typedef struct {
    void (*ws_active_push_reply_callback)(UtilsJsonValue device_ids, int code, void *usr_data);
    void (*ws_message_callback)(UtilsJsonValue type, UtilsJsonValue sub_type, UtilsJsonValue device_id,
                                UtilsJsonValue payload, void *usr_data);
    void (*ws_refresh_http_access_token_reply)(int code, void *usr_data);
} IotWsMessageCallback;

/**
 * @brief Web socket message init.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] callback callback for web socket message
 * @param[in] usr_data usr data for callback
 * @return @see IotReturnCode
 */
int IOT_WsMessage_Init(void *client, IotWsMessageCallback callback, void *usr_data);

/**
 * @brief Unregister web socket message.
 *
 * @param[in,out] client pointer to mqtt client
 */
void IOT_WsMessage_Deinit(void *client);

/**
 * @brief Active ws message or refresh token.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] type @see IotWsMessagePublishType
 * @param[in] access_token access token
 * @return @see IotReturnCode
 */
int IOT_WsMessage_Publish(void *client, IotWsMessagePublishType type, const char *access_token);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_WS_MESSAGE_H_
