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
 * @file data_template_config.c
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

#include "qcloud_iot_data_template_config.h"

/**
 * @brief Set property value.
 *
 * @param[in,out] property pointer to property
 * @param[in] value value to set, should match with property type
 * @return 0 for success.
 */
static int _set_property_value(DataTemplateProperty* property, UtilsJsonValue value)
{
    int i, rc = 0;

    switch (property->type) {
        case DATA_TEMPLATE_TYPE_BOOL:
            if (!strncmp("bool_reverse", value.value, value.value_len)) {
                property->value.value_byte = !property->value.value_byte;
                return 0;
            }
            return utils_json_value_data_get(value, UTILS_JSON_VALUE_TYPE_UINT8, &property->value.value_byte);
        case DATA_TEMPLATE_TYPE_INT:
            return utils_json_value_data_get(value, UTILS_JSON_VALUE_TYPE_INT32, &property->value.value_int);
        case DATA_TEMPLATE_TYPE_UINT:
            return utils_json_value_data_get(value, UTILS_JSON_VALUE_TYPE_UINT32, &property->value.value_uint);
        case DATA_TEMPLATE_TYPE_STRING:
            if (!property->value.value_string.str) {  // no need copy
                return 0;
            }
            strncpy(property->value.value_string.str, value.value_str, value.value_len);
            property->value.value_string.str[value.value_len] = '\0';
            property->value.value_string.str_len              = value.value_len;
            return 0;
        case DATA_TEMPLATE_TYPE_FLOAT:
            return utils_json_value_data_get(value, UTILS_JSON_VALUE_TYPE_FLOAT, &property->value.value_float);
        case DATA_TEMPLATE_TYPE_STRUCT:
            for (i = 0; i < property->value.value_struct.count; i++) {
                DataTemplateProperty* property_struct = property->value.value_struct.property + i;
                UtilsJsonValue        value_struct;
                rc = utils_json_value_get(property_struct->key, strlen(property_struct->key), value.value,
                                          value.value_len, &value_struct);
                if (rc) {
                    break;
                }
                rc = _set_property_value(property_struct, value_struct);
                if (rc) {
                    break;
                }
            }
            return rc;
        case DATA_TEMPLATE_TYPE_ARRAY:
            Log_e("array type is not supported yet!");
            return -1;
        default:
            Log_e("unkown type!");
            return -1;
    }
}

/**
 * @brief Parse property array.
 *
 * @param[in] json_buf json string to parse
 * @param[in] buf_len json len
 * @param[in,out] properties pointer to property array
 * @param[in] property_count count of property
 */
static void _parse_property_array(const char* json_buf, int buf_len, DataTemplateProperty* properties,
                                  int property_count)
{
    DataTemplateProperty* property;
    UtilsJsonValue        value;
    for (int i = 0; i < property_count; i++) {
        property = &properties[i];
        if (!property->is_rw) {  // read only property should not be processed
            continue;
        }
        if (!utils_json_value_get(property->key, strlen(property->key), json_buf, buf_len, &value)) {
            property->is_change = property->need_report = !_set_property_value(property, value);
        }
    }
}

/**************************************************************************************
 * API
 **************************************************************************************/

/**
 * @brief Create data template.
 *
 * @param[in] property_count property count defined in cloud console
 * @param[in] property_data_size size of string property and structure property
 * @param[in] event_count event count defined in cloud console
 * @param[in] action_count action count defined in cloud console
 * @param[in] action_data_size size of string property and structure property in input params
 * @return NULL for fail, @see DataTemplate
 */
DataTemplate* iot_data_template_create(size_t property_count, size_t property_data_size, size_t event_count,
                                       size_t action_count, size_t action_data_size)
{
    DataTemplate* data_template = HAL_Malloc(sizeof(DataTemplate));
    if (!data_template) {
        return NULL;
    }
    memset(data_template, 0, sizeof(DataTemplate));

    if (property_count) {
        data_template->property = HAL_Malloc(sizeof(DataTemplateProperty) * property_count);
        memset(data_template->property, 0, sizeof(DataTemplateProperty) * property_count);
        data_template->property_count = property_count;
    }
    if (property_data_size) {
        data_template->property_data  = HAL_Malloc(property_data_size);
        memset(data_template->property_data, 0, property_data_size);
    }

    if (event_count) {
        data_template->event = HAL_Malloc(sizeof(DataTemplateEvent) * event_count);
        memset(data_template->event, 0, sizeof(DataTemplateEvent) * event_count);
        data_template->event_count = event_count;
    }

    if (action_count) {
        data_template->action = HAL_Malloc(sizeof(DataTemplateAction) * action_count);
        memset(data_template->action, 0, sizeof(DataTemplateAction) * action_count);
        data_template->action_count = action_count;
    }
    if (action_data_size) {
        data_template->action_data  = HAL_Malloc(action_data_size);
        memset(data_template->action_data, 0, action_data_size);
    }

    if ((!data_template->property && property_count) || (!data_template->property_data && property_data_size) ||
        (!data_template->event && event_count) || (!data_template->action && action_count) ||
        (!data_template->action_data && action_data_size)) {
        goto exit;
    }
    return data_template;
exit:
    iot_data_template_destroy(data_template);
    return NULL;
}

/**
 * @brief Destroy data template.
 *
 * @param[in] data_template pointer return by iot_data_template_create
 */
void iot_data_template_destroy(DataTemplate* data_template)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    HAL_Free(data_template->property);
    HAL_Free(data_template->property_data);
    HAL_Free(data_template->event);
    HAL_Free(data_template->action);
    HAL_Free(data_template->action_data);
    HAL_Free(data_template);
}

/**
 * @brief Get property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_property_value_get(DataTemplate* data_template, size_t index)
{
    DataTemplateProperty* property = data_template->property;
    return property[index].value;
}

/**
 * @brief Set property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index index of property
 * @param[in] value @see DataTemplatePropertyValue, @note value should match property type.
 */
void iot_data_template_property_value_set(DataTemplate* data_template, size_t index, DataTemplatePropertyValue value)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    DataTemplateProperty* property = data_template->property;
    if (property[index].type == DATA_TEMPLATE_TYPE_STRING) {
        Log_d("set property %s :%s >>> %s", property[index].key, data_template->property[index].value.value_string.str,
              value.value_string.str);
        strncpy(data_template->property[index].value.value_string.str, value.value_string.str,
                value.value_string.str_len);
        property[index].value.value_string.str[value.value_string.str_len] = '\0';
        property[index].value.value_string.str_len                         = value.value_string.str_len;
    } else {
        if (property[index].type == DATA_TEMPLATE_TYPE_BOOL) {
            Log_d("set property %s :%d >>> %d", property[index].key, property[index].value.value_byte,
                  value.value_byte);
        } else if (property[index].type == DATA_TEMPLATE_TYPE_ENUM) {
            Log_d("set property %s :%d >>> %d", property[index].key, property[index].value.value_uint,
                  value.value_uint);
        } else if (property[index].type == DATA_TEMPLATE_TYPE_FLOAT) {
            Log_d("set property %s :%f >>> %f", property[index].key, property[index].value.value_float,
                  value.value_float);
        } else {
            Log_d("set property %s :%d >>> %d", property[index].key, property[index].value.value_int, value.value_int);
        }
        property[index].value = value;
    }
    property[index].need_report = 1;
}

/**
 * @brief Get property(struct) value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_property_struct_value_get(DataTemplate* data_template, size_t struct_index,
                                                                      int property_index)
{
    DataTemplateProperty* property = data_template->property;
    return property[struct_index].value.value_struct.property[property_index].value;
}

/**
 * @brief Set property(struct) value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @param[in] value  @see DataTemplatePropertyValue, @note value should match property type.
 */
void iot_data_template_property_struct_value_set(DataTemplate* data_template, size_t struct_index,
                                                 size_t property_index, DataTemplatePropertyValue value)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    DataTemplateProperty* property = data_template->property;
    if (property[struct_index].value.value_struct.property[property_index].type == DATA_TEMPLATE_TYPE_STRING) {
        char* dst_str = property[struct_index].value.value_struct.property[property_index].value.value_string.str;
        strncpy(dst_str, value.value_string.str, value.value_string.str_len);
        dst_str[value.value_string.str_len] = '\0';
        property[struct_index].value.value_struct.property[property_index].value.value_string.str_len =
            value.value_string.str_len;
    } else {
        property[struct_index].value.value_struct.property[property_index].value = value;
    }
    property[struct_index].need_report = 1;
}

/**
 * @brief Parse control message and set property value.
 *
 * @param[in,out] data_template usr data template
 * @param[in] params params filed of control message
 */
void iot_data_template_property_parse(DataTemplate* data_template, UtilsJsonValue params)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    POINTER_SANITY_CHECK_RTN(params.value);
    DataTemplateProperty* property = data_template->property;
    _parse_property_array(params.value, params.value_len, property, data_template->property_count);
}

/**
 * @brief Get property status.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return if property is changed
 */
uint8_t iot_data_template_property_status_get(DataTemplate* data_template, size_t index)
{
    POINTER_SANITY_CHECK(data_template, 0);
    DataTemplateProperty* property = data_template->property;
    return property[index].is_change;
}

/**
 * @brief Reset property status.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 */
void iot_data_template_property_status_reset(DataTemplate* data_template, size_t index)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    DataTemplateProperty* property = data_template->property;
    property[index].is_change      = 0;
}

/**
 * @brief Check if need report.
 *
 * @param[in,out] data_template usr data template
 * @return 0: need
 */
int iot_data_template_check_report(DataTemplate* data_template)
{
    POINTER_SANITY_CHECK(data_template, QCLOUD_ERR_FAILURE);
    DataTemplateProperty* property = data_template->property;
    for (int i = 0; i < data_template->property_count; i++) {
        DataTemplateProperty* data_property = &property[i];
        if (data_property->need_report) {
            return QCLOUD_RET_SUCCESS;
        }
    }
    return QCLOUD_ERR_FAILURE;
}

/**
 * @brief Get property type.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return @see DataTemplatePropertyType
 */
DataTemplatePropertyType iot_data_template_property_type_get(DataTemplate* data_template, size_t index)
{
    DataTemplateProperty* property = data_template->property;
    return property[index].type;
}

/**
 * @brief Get property key.
 *
 * @param[in,out] data_template usr data template
 * @param[in] index @see size_t
 * @return key string
 */
const char* iot_data_template_property_key_get(DataTemplate* data_template, size_t index)
{
    POINTER_SANITY_CHECK(data_template, NULL);
    DataTemplateProperty* property = data_template->property;
    return property[index].key;
}

/**
 * @brief Get property(struct) key.
 *
 * @param[in,out] data_template usr data template
 * @param[in] struct_index @note DATA_TEMPLATE_TYPE_STRUCT is required here.
 * @param[in] property_index depends on which struct
 * @return key string
 */
const char* iot_data_template_property_struct_key_get(DataTemplate* data_template, size_t struct_index,
                                                      int property_index)
{
    POINTER_SANITY_CHECK(data_template, NULL);
    DataTemplateProperty* property = data_template->property;
    return property[struct_index].value.value_struct.property[property_index].key;
}

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
                                   size_t* index)
{
    POINTER_SANITY_CHECK(data_template, QCLOUD_ERR_FAILURE);
    DataTemplateAction*   action   = data_template->action;
    DataTemplateProperty* property = NULL;

    int input_property_count;

    for (int i = 0; i < data_template->action_count; i++) {
        if (!strncmp(action_id.value, action[i].action_id, action_id.value_len)) {
            property             = action[i].input_struct.value_struct.property;
            input_property_count = action[i].input_struct.value_struct.count;

            // 1. reset need report
            for (int j = 0; j < input_property_count; j++) {
                property[j].need_report = 0;
            }

            // 2. parse
            _parse_property_array(params.value, params.value_len, property, input_property_count);

            // 3. check all the input params is set
            for (int j = 0; j < input_property_count; j++) {
                if (!property[j].need_report) {
                    return QCLOUD_ERR_JSON_PARSE;
                }
            }
            *index = i;
            return QCLOUD_RET_SUCCESS;
        }
    }
    return QCLOUD_ERR_JSON_PARSE;
}

/**
 * @brief Get input value, should call after iot_data_template_action_parse
 *
 * @param[in,out] data_template usr data template
 * @param[in] index index get from iot_data_template_action_parse
 * @param[in] property_index property index of action input params
 * @return @see DataTemplatePropertyValue
 */
DataTemplatePropertyValue iot_data_template_action_input_value_get(DataTemplate* data_template, size_t index,
                                                                   int property_index)
{
    DataTemplateAction* action = data_template->action;
    return action[index].input_struct.value_struct.property[property_index].value;
}
