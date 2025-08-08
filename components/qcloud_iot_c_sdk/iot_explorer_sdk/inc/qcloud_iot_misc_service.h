/**
 * @file qcloud_iot_misc_service.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-11-24
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
 * 2022-11-24		1.0			hubertxxu		first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_MISC_SERVICE_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_MISC_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief callback of gateway scene
 *
 */
typedef struct {
    void (*device_unbind_reply_callback)(int result, void *usr_data);
    void (*device_unbind_bycloud_callback)(void *usr_data, UtilsJsonValue device_id);
    void (*device_bind_bycloud_callback)(void *usr_data);
} IoTMiscServiceCallback;

/**
 * @brief Actively initiate a request to unbind the device to the platform
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] callbacks @see IoTMiscServiceCallback
 * @param[in] usr_data
 * @return 0 for success, or err code (<0) @see IotReturnCode
 */
int IOT_MiscService_Init(void *client, IoTMiscServiceCallback callbacks, void *usr_data);

/**
 * @brief misc service deinit, unregister handler from server list.
 *
 * @param[in,out] client pointer to mqtt client
 */
void IOT_MiscService_Deinit(void *client);
/**
 * @brief Actively initiate a request to unbind the device to the platform
 *
 * @param[in,out] client pointer to mqtt client
 * @return 0 for success, or err code (<0) @see IotReturnCode
 */
int IOT_MiscService_UnbindDeviceRequest(void *client);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_EXPLORER_QCLOUD_IOT_MISC_SERVICE_H_
