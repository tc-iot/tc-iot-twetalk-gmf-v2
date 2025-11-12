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
 * @file qcloud_iot_data_template_config.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-08-16
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-08-16 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_TCIOT_DATA_TEMPLATE_CONFIG_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_TCIOT_DATA_TEMPLATE_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_common.h"
#include "qcloud_iot_data_template.h"

/**
 * @brief Type of property.
 *
 */
typedef enum {
    DATA_TEMPLATE_TYPE_BOOL,
    DATA_TEMPLATE_TYPE_INT,
    DATA_TEMPLATE_TYPE_STRING,
    DATA_TEMPLATE_TYPE_FLOAT,
    DATA_TEMPLATE_TYPE_UINT,
    DATA_TEMPLATE_TYPE_STRUCT,
    DATA_TEMPLATE_TYPE_ARRAY,
    DATA_TEMPLATE_TYPE_STRING_ENUM,
} DataTemplatePropertyType;

#define DATA_TEMPLATE_TYPE_ENUM        DATA_TEMPLATE_TYPE_UINT
#define DATA_TEMPLATE_TYPE_TIME        DATA_TEMPLATE_TYPE_ENUM
#define DATA_TEMPLATE_TYPE_STRING_ENUM DATA_TEMPLATE_TYPE_STRING

/**
 * @brief Declaration.
 *
 */
typedef struct DataTemplateProperty     DataTemplateProperty;
typedef union DataTemplatePropertyValue DataTemplatePropertyValue;

/**
 * @brief Property value definition.
 *
 */
union DataTemplatePropertyValue {
    uint8_t  value_byte;
    int32_t  value_int;
    uint32_t value_uint;
    float    value_float;
    struct {
        char*  str;
        size_t str_len;
    } value_string;

    struct {
        DataTemplateProperty* property;
        size_t                count;
    } value_struct;
};

/**
 * @brief Property definition.
 *
 */
struct DataTemplateProperty {
    DataTemplatePropertyType  type;
    const char*               key;
    DataTemplatePropertyValue value;
    uint8_t                   need_report;
    uint8_t                   is_change;
    uint8_t                   change_ack;
    uint8_t                   report_ack;
    uint8_t                   is_rw;
};

/**
 * @brief Event definition.
 *
 */
typedef struct {
    const char*               event_id; /**< event id defined in data template */
    IotDataTemplateEventType  type;     /**< event type defined in data template */
    const char*               params;   /**< property json defined in data template */
    DataTemplatePropertyValue params_struct;
} DataTemplateEvent;

/**
 * @brief Action definition.
 *
 */
typedef struct {
    const char*                action_id;
    DataTemplatePropertyValue  input_struct;
    DataTemplatePropertyValue  output_struct;
    IotDataTemplateActionReply reply;
} DataTemplateAction;

/**
 * @brief Data template.
 *
 */
typedef struct {
    DataTemplateProperty* property;
    size_t                property_count;
    void*                 property_data;
    DataTemplateEvent*    event;
    size_t                event_count;
    DataTemplateAction*   action;
    size_t                action_count;
    void*                 action_data;
} DataTemplate;

/**************************************************************************************
 * api for user data template
 **************************************************************************************/

/**
 * @brief Init data template.
 *
 * @param[in] property_count property count defined in cloud console
 * @param[in] property_data_size size of string property and structure property
 * @param[in] event_count event count defined in cloud console
 * @param[in] action_count action count defined in cloud console
 * @param[in] action_data_size size of string property and structure property in input params
 * @return NULL for fail, @see DataTemplate
 */
DataTemplate* iot_data_template_create(size_t property_count, size_t property_data_size, size_t event_count,
                                       size_t action_count, size_t action_data_size);

/**
 * @brief Destroy data template.
 *
 * @param[in] data_template pointer return by iot_data_template_create
 */
void iot_data_template_destroy(DataTemplate* data_template);

/**
 * @brief Get property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_property_value_get(DataTemplate* data_template, size_t index);

/**
 * @brief Set property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index index of property
 * @param[in] value @see DataTemplatePropertyValue, @note value should match property type.
 */
void iot_data_template_property_value_set(DataTemplate* data_template, size_t index, DataTemplatePropertyValue value);

/**
 * @brief Get property(struct) value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_property_struct_value_get(DataTemplate* data_template, size_t struct_index,
                                                                      int property_index);

/**
 * @brief Set property(struct) value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @param[in] value  @see DataTemplatePropertyValue, @note value should match property type.
 */
void iot_data_template_property_struct_value_set(DataTemplate* data_template, size_t struct_index,
                                                 size_t property_index, DataTemplatePropertyValue value);

/**
 * @brief Parse control message and set property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] params params filed of control message
 */
void iot_data_template_property_parse(DataTemplate* data_template, UtilsJsonValue params);

/**
 * @brief Get property status.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return if property is changed
 */
uint8_t iot_data_template_property_status_get(DataTemplate* data_template, size_t index);

/**
 * @brief Reset property status.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 */
void iot_data_template_property_status_reset(DataTemplate* data_template, size_t index);

/**
 * @brief Check if need report.
 *
 * @param[in,out] data_template usr data template
 * @return 0: need
 */
int iot_data_template_check_report(DataTemplate* data_template);

/**
 * @brief Get property type.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return @see DataTemplatePropertyType
 */
DataTemplatePropertyType iot_data_template_property_type_get(DataTemplate* data_template, size_t index);

/**
 * @brief Get property key.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return key string
 */
const char* iot_data_template_property_key_get(DataTemplate* data_template, size_t index);

/**
 * @brief Get property(struct) key.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @return key string
 */
const char* iot_data_template_property_struct_key_get(DataTemplate* data_template, size_t struct_index,
                                                      int property_index);

/**
 * @brief Report all the properties needed report.
 *
 * @param[in,out] data_template usr data template
 * @param[in,out] client pointer to mqtt client
 * @param[in] buf buffer to report
 * @param[in] buf_len buffer length
 * @return packet id (>=0) when success, or err code (<0) @see IotReturnCode
 */
int iot_data_template_property_report(DataTemplate* data_template, void* client, char* buf, int buf_len);

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
                                  const char* params);

/**
 * @brief Parse action message and set action input params.
 *
 * @param[in,out] data_template usr data template
 * @param[in] action_id action id
 * @param[in] params params of action
 * @param[out] index @see size_t
 * @return 0 for success, QCLOUD_ERR_JSON_PARSE for invalid json.
 */
int iot_data_template_action_parse(DataTemplate* data_template, UtilsJsonValue action_id, UtilsJsonValue params,
                                   size_t* index);

/**
 * @brief Get input value, should call after iot_data_template_action_parse
 *
 * @param[in,out] data_template usr data template
 * @param[in] index index get from iot_data_template_action_parse
 * @param[in] property_index property index of action input params
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_action_input_value_get(DataTemplate* data_template, size_t index,
                                                                   int property_index);

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
                                   UtilsJsonValue client_token, int code, const char* response);

/**************************************************************************************
 * lltlv
 **************************************************************************************/

/**
 * @brief Construct tlv base data template.
 *
 * @param[in,out] data_template usr data template
 * @param[out] buf buffer to store tlv
 * @param[in] buf_len buffer max length
 * @param[in] is_control
 * @return tlv length
 */
size_t iot_data_template_tlv_construct(DataTemplate* data_template, uint8_t* buf, size_t buf_len, uint8_t is_control);

/**
 * @brief Parse tlv data to data template.
 *
 * @param[in,out] data_template usr data template
 * @param[in] data tlv data
 * @param[in] data_len tlv data length
 * @param[in] is_control
 * @return 0 is success
 */
int iot_data_template_tlv_parse(DataTemplate* data_template, uint8_t* data, size_t data_len, uint8_t is_control);

/**
 * @brief Set ack.
 *
 * @param[in,out] data_template usr data template
 * @param[in] is_control
 */
void iot_data_template_tlv_ack(DataTemplate* data_template, uint8_t is_control);

/**
 * @brief Parse control message and set property value.
 *
 * @param[in,out] data_template usr data template
 * @param[out] buf buffer to store tlv
 * @param[in] buf_len buffer max length
 * @param[in] params params filed of control message
 * @return size_t
 */
size_t iot_data_template_tlv_json_parse(DataTemplate* data_template, uint8_t* buf, size_t buf_len,
                                        UtilsJsonValue params);

/**
 * @brief Parse property from console data template json string.
 *
 * @param[in,out] data_template usr data template
 * @param[in] params params filed of property json string
 */
int iot_data_template_console_json_parse_property(DataTemplate* data_template, UtilsJsonValue params);

/**
 * @brief free property from console data template json string.
 *
 * @param[in,out] data_template usr data template
 */
void iot_data_template_console_json_free_property(DataTemplate* data_template);

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_COMPONENT_QCLOUD_TCIOT_DATA_TEMPLATE_CONFIG_H_
