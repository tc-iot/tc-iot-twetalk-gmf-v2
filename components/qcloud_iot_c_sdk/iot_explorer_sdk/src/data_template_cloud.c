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
 * @file data_template_cloud.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-10-12
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-10-12 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_data_template_config.h"

/**
 * @brief Get property node in json.
 *
 * @param[out] json_buf buffer to put node
 * @param[in] buf_len buffer length
 * @param[in] property property to get node
 * @return 0 for success.
 */
static int _get_property_node(char* json_buf, int buf_len, const DataTemplateProperty* property)
{
    int len, i;
    switch (property->type) {
        case DATA_TEMPLATE_TYPE_INT:
            return TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":%d", property->key, property->value.value_int);
        case DATA_TEMPLATE_TYPE_BOOL:
            return TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":%u", property->key, property->value.value_byte);
        case DATA_TEMPLATE_TYPE_UINT:
            return TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":%u", property->key, property->value.value_uint);
        case DATA_TEMPLATE_TYPE_STRING:
            if (!property->value.value_string.str) {
                return 0;
            }
            return TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":\"%s\"", property->key, property->value.value_string);
        case DATA_TEMPLATE_TYPE_FLOAT:
            return TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":%f", property->key, property->value.value_float);
        case DATA_TEMPLATE_TYPE_STRUCT:
            len = TCI_HAL_Snprintf(json_buf, buf_len, "\"%s\":{", property->key);
            for (i = 0; i < property->value.value_struct.count; i++) {
                len += _get_property_node(json_buf + len, buf_len - len, property->value.value_struct.property + i);
                json_buf[len++] = ',';
            }
            json_buf[--len] = '}';
            return len + 1;
        case DATA_TEMPLATE_TYPE_ARRAY:
            Log_e("array type is not supported yet!");
            return 0;
        default:
            Log_e("unkown type!");
            return 0;
    }
}

/**
 * @brief Reply action, should call after parse action message.
 *
 * @param[in,out] data_template usr data template
 * @param[in,out] client pointer to mqtt client
 * @param[in] buf buffer to report
 * @param[in] buf_len buffer length
 * @param[in] index @see size_t
 * @param[in] client_token client token of action message
 * @param[in] code result code for action, 0 for success
 * @param[in] response action output params, user should construct params according to action
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int iot_data_template_action_reply(DataTemplate* data_template, void* client, char* buf, int buf_len, size_t index,
                                   UtilsJsonValue client_token, int code, const char* response)
{
    POINTER_SANITY_CHECK(data_template, QCLOUD_ERR_FAILURE);
    DataTemplateAction* action       = data_template->action;
    action[index].reply.code         = code;
    action[index].reply.client_token = client_token;
    action[index].reply.response     = response;
    return TCIOT_DataTemplate_ActionReply(client, buf, buf_len, action[index].reply);
}

/**
 * @brief Report all the properties needed report.
 *
 * @param[in,out] data_template usr data template
 * @param[in,out] client pointer to mqtt client
 * @param[in] buf buffer to report
 * @param[in] buf_len buffer length
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int iot_data_template_property_report(DataTemplate* data_template, void* client, char* buf, int buf_len)
{
    POINTER_SANITY_CHECK(data_template, QCLOUD_ERR_FAILURE);
    DataTemplateProperty* property = data_template->property;

    char params[1024];
    memset(params, 0, sizeof(params));
    params[0] = '{';
    int offset, param_offset = 1;
    for (int i = 0; i < data_template->property_count; i++) {
        DataTemplateProperty* data_property = &property[i];
        if (data_property->need_report) {
            offset = _get_property_node(params + param_offset, sizeof(params) - param_offset, data_property);
            if (offset) {
                param_offset += offset;
                params[param_offset++] = ',';
            }
            data_property->need_report = 0;
        }
    }
    params[--param_offset] = '}';

    if (param_offset) {
        return TCIOT_DataTemplate_PropertyReport(client, buf, buf_len, params);
    }
    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief Post event.
 *
 * @param[in,out] data_template usr data template
 * @param[in,out] client pointer to mqtt client
 * @param[in] buf buffer to report
 * @param[in] buf_len buffer length
 * @param[in] event_id event id
 * @param[in] params user should construct params according to event.
 */
void iot_data_template_event_post(DataTemplate* data_template, void* client, char* buf, int buf_len, size_t event_id,
                                  const char* params)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    DataTemplateEvent* event = data_template->event;
    if (params) {
        event[event_id].params = params;
    }
    IotDataTemplateEventData data = {
        .event_id = event[event_id].event_id,
        .params   = event[event_id].params,
        .type     = event[event_id].type,
    };
    TCIOT_DataTemplate_EventPost(client, buf, buf_len, data);
}
