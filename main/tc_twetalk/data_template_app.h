/**
 * @file data_template_app.h
 * @author hubertxxu (hubertxxu@tencent.com)
 * @brief 
 * @version 0.1
 * @date 2025-07-30
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

#ifndef IOT_HUB_DEVICE_C_SDK_APP_TWETALK_DATA_TEMPLATE_APP_H_
#define IOT_HUB_DEVICE_C_SDK_APP_TWETALK_DATA_TEMPLATE_APP_H_

#include "qcloud_iot_common.h"
#include "qcloud_iot_explorer.h"
#include "qcloud_iot_data_template_config.h"

int usr_data_template_init(void *mqtt_client);

int usr_data_template_deinit(void *mqtt_client);

int usr_report_battery(void *mqtt_client, int battery);

int usr_report_volume(void *mqtt_client, int volume);

#endif // IOT_HUB_DEVICE_C_SDK_APP_TWETALK_DATA_TEMPLATE_APP_H_