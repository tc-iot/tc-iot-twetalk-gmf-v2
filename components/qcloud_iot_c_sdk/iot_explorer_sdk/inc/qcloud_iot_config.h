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
 * @file qcloud_iot_config.h
 * @brief sdk config define
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-06-01
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-06-01 <td>1.0     <td>fancyxu   <td>first commit
 * <tr><td>2021-07-12 <td>1.1     <td>fancyxu   <td>rename AUTH_WITH_NOTLS to AUTH_WITH_NO_TLS
 *
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_CONFIG_QCLOUD_TCIOT_CONFIG_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_CONFIG_QCLOUD_TCIOT_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define QCLOUD_TCIOT_DEVICE_SDK_VERSION "4.1.0-d4b0d876d8fa46e011d99dfccf7002333184478a"
/* #undef ENABLE_AUTH_MODE_CERT */
#define ENABLE_AUTH_MODE_KEY
#define ENABLE_AUTH_NO_TLS
/* #undef ENABLE_GATEWAY */
#define ENABLE_DYNAMIC_DEVICE_REG
/* #undef ENABLE_LOG_UPLOAD */
/* #undef ENABLE_SDK_DEBUG */
/* #undef ENABLE_DEBUG_DEVICE_INFO */
/* #undef ENABLE_MODULE_AT */
/* #undef ENABLE_LOG_UPLOAD_JSON */
/* #undef ENABLE_LOG_UPLOAD_AES */
#define ENABLE_WIFI_CONFIG
/* #undef ENABLE_WIFI_SOFT_AP */
#define ENABLE_WIFI_BLE_LLSYNC

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_CONFIG_QCLOUD_TCIOT_CONFIG_H_
