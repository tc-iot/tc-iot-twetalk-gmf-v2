/**
 * @file data_template_app.c
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

#include "data_template_app.h"
#include "data_template_config.include"

static DataTemplate *sg_data_template;

static void _handle_property_callback(void *client, int is_get_status)
{
    for (UsrPropertyIndex i = USR_PROPERTY_INDEX_BATTERY; i <= USR_PROPERTY_INDEX_VOLUME; i++) {
        if (iot_data_template_property_status_get(sg_data_template, i)) {
            DataTemplatePropertyValue value;
            switch ((int)i) {
                case USR_PROPERTY_INDEX_VOLUME:
                    value = iot_data_template_property_value_get(sg_data_template, i);
                    Log_d("recv %s:%d", iot_data_template_property_key_get(sg_data_template, i), value.value_int);
                    // TODO 处理音量变化
                    extern void audio_set_volume(int volume);
                    audio_set_volume(value.value_int);
                    usr_report_volume(client, value.value_int);
                    break;
            }
            iot_data_template_property_status_reset(sg_data_template, i);
        }
    }
}

static void _method_control_callback(UtilsJsonValue client_token, UtilsJsonValue params, void *iot_data)
{
    char buf[256];
    Log_i("recv msg[%.*s]: params=%.*s", client_token.value_len, client_token.value, params.value_len, params.value);
    iot_data_template_property_parse(sg_data_template, params);
    _handle_property_callback(iot_data, 0);
    IOT_DataTemplate_PropertyControlReply(iot_data, buf, sizeof(buf), 0, client_token);
}

static void _method_get_status_reply_callback(UtilsJsonValue client_token, int code, UtilsJsonValue reported,
                                              UtilsJsonValue control, void *iot_data)
{
    char buf[256];
    Log_i("recv msg[%.*s]: code=%d|reported=%.*s|control=%.*s", client_token.value_len, client_token.value, code,
          reported.value_len, STRING_PTR_PRINT_SANITY_CHECK(reported.value), control.value_len,
          STRING_PTR_PRINT_SANITY_CHECK(control.value));
    iot_data_template_property_parse(sg_data_template, control);
    _handle_property_callback(iot_data, 1);
    IOT_DataTemplate_PropertyClearControl(iot_data, buf, sizeof(buf));
}

static void _method_action_callback(UtilsJsonValue client_token, UtilsJsonValue action_id, UtilsJsonValue params,
                                    void *iot_data)
{
    size_t                    index;
    int                       rc;

    Log_i("recv msg[%.*s]: action_id=%.*s|params=%.*s", client_token.value_len, client_token.value, action_id.value_len,
          action_id.value, params.value_len, params.value);

    rc = iot_data_template_action_parse(sg_data_template, action_id, params, &index);
    if (rc) {
        return;
    }

    switch (index) {
            break;
        default:
            break;
    }
}

int usr_data_template_init(void *mqtt_client)
{
    int rc = 0;
    if(mqtt_client == NULL) {
        Log_e("mqtt client is NULL");
        return QCLOUD_ERR_INVAL;
    }
    sg_data_template = _usr_data_template_create();
    if (!sg_data_template) {
        return QCLOUD_ERR_MALLOC;
    }

    // subscribe normal topics and wait result
    IotDataTemplateCallback callback                            = DEFAULT_DATA_TEMPLATE_CALLBACK;
    callback.property_callback.method_control_callback          = _method_control_callback;
    callback.property_callback.method_get_status_reply_callback = _method_get_status_reply_callback;
    callback.action_callback.method_action_callback             = _method_action_callback;

    rc = IOT_DataTemplate_Init(mqtt_client, callback, mqtt_client);
    if (rc) {
        Log_e("Client Subscribe Topic Failed: %d", rc);
        return rc;
    }
    return QCLOUD_RET_SUCCESS;
}

int usr_data_template_deinit(void *mqtt_client)
{
    IOT_DataTemplate_Deinit(mqtt_client);
    iot_data_template_destroy(sg_data_template);
    sg_data_template = NULL;
    return QCLOUD_RET_SUCCESS;
}

int usr_report_battery(void *mqtt_client, int battery)
{
    if(sg_data_template == NULL || mqtt_client == NULL) {
        Log_e("data template is NULL");
        return QCLOUD_ERR_INVAL;
    }
    DataTemplatePropertyValue value;
    value.value_uint = battery;
    iot_data_template_property_value_set(sg_data_template, USR_PROPERTY_INDEX_BATTERY, value);
    char buf[256];
    return iot_data_template_property_report(sg_data_template, mqtt_client, buf, sizeof(buf));
}

int usr_report_volume(void *mqtt_client, int volume)
{
    if(sg_data_template == NULL || mqtt_client == NULL) {
        Log_e("data template is NULL");
        return QCLOUD_ERR_INVAL;
    }
    DataTemplatePropertyValue value;
    value.value_int = volume;
    iot_data_template_property_value_set(sg_data_template, USR_PROPERTY_INDEX_VOLUME, value);
    char buf[256];
    return iot_data_template_property_report(sg_data_template, mqtt_client, buf, sizeof(buf));
}


