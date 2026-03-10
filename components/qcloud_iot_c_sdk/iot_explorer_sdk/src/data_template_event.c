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
 * @file data_template_event.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-09-26
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-09-26 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "data_template.h"

typedef struct {
    UtilsJsonValue client_token;
    int            code;
} EventParams;

static UtilsListResult _event_callback(void* list, void* val, void* usr_data)
{
    DataTemplateContext* context      = (DataTemplateContext*)val;
    EventParams*         event_params = (EventParams*)usr_data;
    context->event_callback.method_event_reply_callback(event_params->client_token, event_params->code,
                                                        context->usr_data);
    return LIST_TRAVERSE_CONTINUE;
}

/**
 * @brief Mqtt message callback for event topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] message message from topic
 * @param[in,out] usr_data pointer to @see DataTemplateContext
 */
void data_template_event_message_handler(void* client, const MQTTMessage* message, void* usr_data)
{
    int rc;

    EventParams event_params = {0};
    Log_d("receive event message:%.*s", message->payload_len, message->payload_str);

    rc = utils_json_check_value("method", sizeof("method") - 1, "event_reply", sizeof("event_reply") - 1,
                                message->payload_str, message->payload_len);
    if (rc) {
        goto error;
    }

    rc = utils_json_value_get("clientToken", strlen("clientToken"), message->payload_str, message->payload_len,
                              &event_params.client_token);
    if (rc) {
        goto error;
    }

    rc = utils_json_get_int("code", strlen("code"), message->payload_str, message->payload_len, &event_params.code);
    if (rc) {
        goto error;
    }

    utils_list_process(usr_data, LIST_HEAD, _event_callback, &event_params);
    return;
error:
    Log_e("invalid format of payload!");
}

/**
 * @brief Publish message to event topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[out] buf publish message buffer
 * @param[in] buf_len buffer len
 * @param[in] data @see IotDataTemplateEventData
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int data_template_event_reply_publish(void* client, char* buf, int buf_len, IotDataTemplateEventData data)
{
    const char* event_type[] = {
        [TCIOT_DATA_TEMPLATE_EVENT_TYPE_INFO]  = "info",
        [TCIOT_DATA_TEMPLATE_EVENT_TYPE_ALERT] = "alert",
        [TCIOT_DATA_TEMPLATE_EVENT_TYPE_FAULT] = "fault",
    };

    int len = TCI_HAL_Snprintf(
        buf, buf_len,
        "{\"method\":\"event_post\",\"clientToken\":\"event-%d\"\",\"eventId\":\"%s\",\"type\":\"%s\",\"params\":%s}",
        TCI_HAL_Random(), data.event_id, event_type[data.type], data.params);
    return data_template_publish(client, DATA_TEMPLATE_TYPE_EVENT, QOS0, buf, len);
}
