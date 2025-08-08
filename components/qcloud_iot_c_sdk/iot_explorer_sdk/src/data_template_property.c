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
 * @file data_template_property.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-08-23
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-08-23 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_data_template.h"
#include "data_template.h"

/**
 * @brief Down method type.
 *
 */
typedef enum {
    PROPERTY_DOWN_METHOD_TYPE_CONTROL = 0,
    PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY,
    PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY,
    PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY,
    PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY,
    PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY,
} PropertyDownMethodType;

static int _check_callback(PropertyDownMethodType type, const PropertyMessageCallback *callback)
{
    void *callback_list[] = {
        [PROPERTY_DOWN_METHOD_TYPE_CONTROL]               = callback->method_control_callback,
        [PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY]          = callback->method_report_reply_callback,
        [PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY]      = callback->method_get_status_reply_callback,
        [PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY]     = callback->method_report_info_reply_callback,
        [PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY]   = callback->method_clear_control_reply_callback,
        [PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY] = callback->method_get_mode_define_reply_callback,
    };

    return callback_list[type] ? 1 : 0;
}

typedef struct {
    PropertyDownMethodType type;
    UtilsJsonValue         client_token;
    int                    code;
    union {
        struct {
            UtilsJsonValue params;
        } control;

        struct {
            UtilsJsonValue reported;
            UtilsJsonValue control;
        } get_status_reply;
        struct {
            UtilsJsonValue property;
            UtilsJsonValue event;
            UtilsJsonValue action;
        } get_model_define;
    };
} PropertyParams;

static UtilsListResult _property_callback(void *list, void *val, void *usr_data)
{
    DataTemplateContext     *context         = (DataTemplateContext *)val;
    PropertyParams          *property_params = (PropertyParams *)usr_data;
    PropertyMessageCallback *callback        = &context->property_callback;

    if (!_check_callback(property_params->type, callback)) {
        return LIST_TRAVERSE_CONTINUE;
    }
    switch (property_params->type) {
        case PROPERTY_DOWN_METHOD_TYPE_CONTROL:
            callback->method_control_callback(property_params->client_token, property_params->control.params,
                                              context->usr_data);
            break;
        case PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY:
            callback->method_report_reply_callback(property_params->client_token, property_params->code,
                                                   context->usr_data);
            break;
        case PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY:
            callback->method_get_status_reply_callback(property_params->client_token, property_params->code,
                                                       property_params->get_status_reply.reported,
                                                       property_params->get_status_reply.control, context->usr_data);
            break;
        case PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY:
            callback->method_report_info_reply_callback(property_params->client_token, property_params->code,
                                                        context->usr_data);
            break;
        case PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY:
            callback->method_clear_control_reply_callback(property_params->client_token, property_params->code,
                                                          context->usr_data);
            break;
        case PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY:
            callback->method_get_mode_define_reply_callback(
                property_params->client_token, property_params->get_model_define.property,
                property_params->get_model_define.event, property_params->get_model_define.action, context->usr_data);
            break;
        default:
            break;
    }
    return LIST_TRAVERSE_CONTINUE;
}

/**
 * @brief Parse payload and callback.
 *
 * @param[in] type @see PropertyDownMethodType
 * @param[in] message message from cloud
 * @param[in,out] usr_data user data used in callback
 */
static void _parse_method_payload_and_callback(PropertyDownMethodType type, const MQTTMessage *message, void *usr_data)
{
    int            rc              = 0;
    PropertyParams property_params = {0};
    property_params.type           = type;

    // get client token
    rc = utils_json_value_get("clientToken", sizeof("clientToken") - 1, message->payload_str, message->payload_len,
                              &property_params.client_token);
    if (rc) {
        goto error;
    }

    // get code
    if (PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY == type || PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY == type ||
        PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY == type || PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY == type) {
        rc = utils_json_get_int("code", sizeof("code") - 1, message->payload_str, message->payload_len,
                                &property_params.code);
        if (rc) {
            goto error;
        }
    }

    // callback
    switch (type) {
        case PROPERTY_DOWN_METHOD_TYPE_CONTROL:
            rc = utils_json_value_get("params", sizeof("params") - 1, message->payload_str, message->payload_len,
                                      &property_params.control.params);
            if (rc) {
                goto error;
            }
            break;
        case PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY:
        case PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY:
        case PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY:
            break;
        case PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY:
            rc = utils_json_value_get("data.reported", sizeof("data.reported") - 1, message->payload_str,
                                      message->payload_len, &property_params.get_status_reply.reported);
            rc &= utils_json_value_get("data.control", sizeof("data.control") - 1, message->payload_str,
                                       message->payload_len, &property_params.get_status_reply.control);
            if (rc) {
                goto error;
            }
            break;
        case PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY:
            utils_json_value_get("model.properties", sizeof("model.properties") - 1, message->payload_str,
                                 message->payload_len, &property_params.get_model_define.property);
            utils_json_value_get("model.events", sizeof("model.events") - 1, message->payload_str, message->payload_len,
                                 &property_params.get_model_define.event);
            utils_json_value_get("model.actions", sizeof("model.actions") - 1, message->payload_str,
                                 message->payload_len, &property_params.get_model_define.action);
            break;
        default:
            goto error;
    }
    utils_list_process(usr_data, LIST_HEAD, _property_callback, &property_params);
    return;
error:
    Log_e("invalid format of payload!");
    return;
}

/**
 * @brief Mqtt message callback for property topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] message message from topic
 * @param[in,out] usr_data pointer to @see DataTemplateContext
 */
void data_template_property_message_handler(void *client, const MQTTMessage *message, void *usr_data)
{
    const char *property_down_method_str[] = {
        [PROPERTY_DOWN_METHOD_TYPE_CONTROL]               = "control",
        [PROPERTY_DOWN_METHOD_TYPE_REPORT_REPLY]          = "report_reply",
        [PROPERTY_DOWN_METHOD_TYPE_GET_STATUS_REPLY]      = "get_status_reply",
        [PROPERTY_DOWN_METHOD_TYPE_REPORT_INFO_REPLY]     = "report_info_reply",
        [PROPERTY_DOWN_METHOD_TYPE_CLEAR_CONTROL_REPLY]   = "clear_control_reply",
        [PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY] = "get_model_define_reply",
    };

    int rc;
    Log_d("receive property message:%.*s", message->payload_len, message->payload_str);
    UtilsJsonValue method;
    rc = utils_json_value_get("method", strlen("method"), message->payload_str, message->payload_len, &method);
    if (rc) {
        return;
    }

    PropertyDownMethodType i;
    for (i = PROPERTY_DOWN_METHOD_TYPE_CONTROL; i <= PROPERTY_DOWN_METHOD_TYPE_GET_MODE_DEFINE_REPLY; i++) {
        if (!strncmp(method.value, property_down_method_str[i], method.value_len)) {
            _parse_method_payload_and_callback(i, message, usr_data);
        }
    }
}

/**
 * @brief Publish message to property topic.
 *
 * @param[in,out] client pointer to mqtt client
 * @param[in] publish_type @see PropertyUpMethodType
 * @param[out] buf publish message buffer
 * @param[in] buf_len buffer len
 * @param[in] params @see PropertyPublishParams
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int data_template_property_publish(void *client, PropertyUpMethodType publish_type, char *buf, int buf_len,
                                   PropertyPublishParams params)
{
    int len = 0;

    const char *method_str[] = {
        [PROPERTY_UP_METHOD_TYPE_REPORT]          = "report",
        [PROPERTY_UP_METHOD_TYPE_REPORT_INFO]     = "report_info",
        [PROPERTY_UP_METHOD_TYPE_GET_STATUS]      = "get_status",
        [PROPERTY_UP_METHOD_TYPE_CLEAR_CONTROL]   = "clear_control",
        [PROPERTY_UP_METHOD_TYPE_CONTROL_REPLY]   = "control_reply",
        [PROPERTY_UP_METHOD_TYPE_GET_MODE_DEFINE] = "get_model_define",
    };

    len = HAL_Snprintf(buf, buf_len, "{\"method\":\"%s\",", method_str[publish_type]);
    switch (publish_type) {
        case PROPERTY_UP_METHOD_TYPE_REPORT:
        case PROPERTY_UP_METHOD_TYPE_REPORT_INFO:
            len += HAL_Snprintf(buf + len, buf_len - len, "\"params\":%s,", params.json);
        case PROPERTY_UP_METHOD_TYPE_GET_STATUS:
        case PROPERTY_UP_METHOD_TYPE_GET_MODE_DEFINE:
        case PROPERTY_UP_METHOD_TYPE_CLEAR_CONTROL:
            len += HAL_Snprintf(buf + len, buf_len - len, "\"clientToken\":\"clear-control-%" SCNu64 "\"", IOT_Timer_CurrentSec());
            break;
        case PROPERTY_UP_METHOD_TYPE_CONTROL_REPLY:
            len += HAL_Snprintf(buf + len, buf_len - len, "\"clientToken\":\"%.*s\",\"code\":%d",
                                params.control_reply.client_token.value_len, params.control_reply.client_token.value,
                                params.control_reply.code);
            break;
        default:
            return QCLOUD_ERR_FAILURE;
    }
    buf[len++] = '}';
    return data_template_publish(client, DATA_TEMPLATE_TYPE_PROPERTY, QOS0, buf, len);
}
