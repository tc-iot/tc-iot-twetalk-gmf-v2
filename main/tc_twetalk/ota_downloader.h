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
 * @file ota_downloader.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-10-20
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-10-20 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_APP_OTA_OTA_DOWNLOADER_H_
#define IOT_HUB_DEVICE_C_SDK_APP_OTA_OTA_DOWNLOADER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define MAX_SIZE_OF_FW_VERSION         32
#define MAX_SIZE_OF_DOWNLOAD_URL       512
#define MAX_SIZE_OF_RESOURCE_FILE_PATH 128
#define RESOURCE_HTTP_BUF_SIZE         4096
#define RESOURCE_HTTP_TIMEOUT_MS       5000

typedef struct {
    int (*on_download_finish)( const char* version, size_t total_len);
    const char* (*get_firmware_version)(void);
} IotOtaInitParams;

int iot_ota_init(void* client, const IotOtaInitParams* params);

int iot_ota_report_version(void);

void iot_ota_process(void* param);

void iot_ota_deinit(void);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_APP_OTA_OTA_DOWNLOADER_H_
