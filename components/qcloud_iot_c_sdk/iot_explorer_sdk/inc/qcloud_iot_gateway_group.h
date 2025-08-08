/**
 * @file qcloud_iot_gateway_group.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-06-24
 *
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
 * @par Change Log:
 * <table>
 * Date				Version		Author			Description
 * 2022-06-24		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_GATEWAY_GROUP_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_GATEWAY_GROUP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "utils_json.h"

/**
 * @brief callback of gateway scene
 *
 */
typedef struct {
    void (*gateway_group_devices_callback)(UtilsJsonValue group_id, UtilsJsonValue devices_array, void *usr_data);
    void (*gateway_delete_group_callback)(UtilsJsonValue group_id, void *usr_data);
    void (*gateway_reload_group_devices_reply_callback)(int result_code, UtilsJsonValue group_result, void *usr_data);
    void (*gateway_group_control_callback)(UtilsJsonValue group_id, UtilsJsonValue control_data, void *usr_data);
} IoTGatewayGroupCallback;

/**
 * @brief   gateway group init, register handler to server list.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] callback @see IoTGatewayGroupCallback
 * @param[in] usr_data usr data used in callback
 * @return 0 for success, or err code (<0) @see IotReturnCode
 */
int IOT_GatewayGroup_Init(void *client, IoTGatewayGroupCallback callback, void *usr_data);

/**
 * @brief Gateway group deinit, unregister handler from server list.
 *
 * @param[in,out] client pointer to mqtt client
 */
void IOT_GatewayGroup_Deinit(void *client);

/**
 * @brief reload gateway group devices from cloud.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[out] buf publish message buffer
 * @param[in] buf_len buffer len
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int IOT_GatewayGroup_Reload(void *client, char *buf, int buf_len);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_GATEWAY_GROUP_H_
