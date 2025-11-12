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
 * @file data_template_parse.c
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-10-14
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-10-14 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#include "qcloud_iot_data_template_config.h"

#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID           "id"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MODE         "mode"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS "define.specs"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE         "define.type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX          "define.max"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STR_LENMAX   2048
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRENUM_LEN  64
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_LEN    512
#define DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS              "properties"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE  "dataType.type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX   "dataType.max"

static char* _data_template_console_json_get_id(DataTemplateProperty* parent_property, UtilsJsonValue params)
{
    char* key     = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID;
    int   key_len = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID) - 1;

    UtilsJsonValue id;
    int            ret = utils_json_value_get(key, key_len, params.value, params.value_len, &id);
    if (ret) {
        /*  */
        Log_e("%s json get failed %d", key, ret);
        return NULL;
    }

    char* value = (char*)TCI_HAL_Malloc(id.value_len + 1);
    if (!value) {
        Log_e("%s json malloc failed", key);
        return NULL;
    }
    memcpy(value, id.value, id.value_len);
    value[id.value_len] = '\0';
    return value;
}

static int _data_template_console_json_get_type(DataTemplateProperty* parent_property, UtilsJsonValue params)
{
    char*                    key         = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE;
    int                      key_len     = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE) - 1;
    DataTemplatePropertyType parent_type = parent_property->type;

    if (parent_type == DATA_TEMPLATE_TYPE_STRUCT) {
        key     = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE;
        key_len = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE) - 1;
    }

    UtilsJsonValue type_value;
    int            ret = utils_json_value_get(key, key_len, params.value, params.value_len, &type_value);
    if (ret) {
        /*  */
        Log_e("%s json get failed %d", key, ret);
        return QCLOUD_ERR_FAILURE;
    }

    const char*              type_str_array[] = {"bool",      "int",    "string", "float",     "enum",
                                                 "timestamp", "struct", "array",  "stringenum"};
    DataTemplatePropertyType type_array[]     = {
        DATA_TEMPLATE_TYPE_BOOL,   DATA_TEMPLATE_TYPE_INT,   DATA_TEMPLATE_TYPE_STRING,
        DATA_TEMPLATE_TYPE_FLOAT,  DATA_TEMPLATE_TYPE_ENUM,  DATA_TEMPLATE_TYPE_TIME,
        DATA_TEMPLATE_TYPE_STRUCT, DATA_TEMPLATE_TYPE_ARRAY, DATA_TEMPLATE_TYPE_STRING_ENUM,
    };

    for (int i = 0; i < sizeof(type_str_array) / sizeof(type_str_array[0]); i++) {
        if (!strncmp(type_str_array[i], type_value.value, type_value.value_len)) {
            return type_array[i];
        }
    }

    Log_e("%.*s type get failed %d", type_value.value_len, type_value.value, ret);

    return QCLOUD_ERR_FAILURE;
}

static int _data_template_console_json_get_rw(DataTemplateProperty* parent_property, UtilsJsonValue params)
{
    char*                    key         = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MODE;
    int                      key_len     = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MODE) - 1;
    DataTemplatePropertyType parent_type = parent_property->type;

    if (parent_type == DATA_TEMPLATE_TYPE_STRUCT) {
        return parent_property->is_rw;
    }

    UtilsJsonValue rw_value;
    int            ret = utils_json_value_get(key, key_len, params.value, params.value_len, &rw_value);
    if (ret) {
        /*  */
        Log_e("%s json get failed %d", key, ret);
        return QCLOUD_ERR_FAILURE;
    }

    char* mode_str_array[] = {"r", "rw"};

    for (int i = 0; i < sizeof(mode_str_array) / sizeof(mode_str_array[0]); i++) {
        if (!strncmp(mode_str_array[i], rw_value.value, rw_value.value_len)) {
            return i;
        }
    }

    Log_e("%.*s rw mode get failed %d", rw_value.value_len, rw_value.value, ret);
    return QCLOUD_ERR_FAILURE;
}

static char* _data_template_console_json_get_strbuff(DataTemplateProperty* parent_property, UtilsJsonValue params)
{
    char*                    key         = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX;
    int                      key_len     = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX) - 1;
    DataTemplatePropertyType parent_type = parent_property->type;

    if (parent_type == DATA_TEMPLATE_TYPE_STRUCT) {
        key     = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX;
        key_len = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX) - 1;
    }

    size_t max_value = 0;
    int    ret       = utils_json_get_int(key, key_len, params.value, params.value_len, (int*)&max_value);
    if (ret || max_value > DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STR_LENMAX) {
        if (parent_type == DATA_TEMPLATE_TYPE_STRING_ENUM) {
            return TCI_HAL_Malloc(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRENUM_LEN + 1);
        }
        Log_e("%s json get failed %d, %d", key, ret, max_value);
        return NULL;
    }
    return TCI_HAL_Malloc(max_value + 1);
}

static int _data_template_console_json_get_property(DataTemplateProperty* parent_property, UtilsJsonValue params);

static UtilsJsonArrayIterResult _data_template_console_json_array_property_callback(UtilsJsonValue params,
                                                                                    void*          user_data)
{
    DataTemplateProperty* parent_property = (DataTemplateProperty*)user_data;
    int                   ret             = QCLOUD_RET_SUCCESS;

    /* parse property */
    DataTemplateProperty* property =
        parent_property->value.value_struct.property + parent_property->value.value_struct.count;

    /* default value */
    property->value.value_byte = 0;

    /* get key */
    property->key = _data_template_console_json_get_id(parent_property, params);
    if (!property->key) {
        Log_e("json get key failed");
        return UTILS_JSON_ARRAY_ITER_STOP;
    }

    /* get read wrtie mode */
    int rw = _data_template_console_json_get_rw(parent_property, params);
    if (rw == QCLOUD_ERR_FAILURE) {
        /*  */
        Log_e("json get rw mode failed");
        return UTILS_JSON_ARRAY_ITER_STOP;
    }

    property->is_rw = rw;

    /* get type */
    int type = _data_template_console_json_get_type(parent_property, params);
    if (type == QCLOUD_ERR_FAILURE) {
        /*  */
        Log_e("json get type failed");
        return UTILS_JSON_ARRAY_ITER_STOP;
    }
    property->type = type;

    switch (property->type) {
        case DATA_TEMPLATE_TYPE_STRING:
            /* type is string or stringenum malloc */
            property->value.value_string.str = _data_template_console_json_get_strbuff(parent_property, params);
            if (!property->value.value_string.str) {
                Log_e("json get string buff failed");
                return UTILS_JSON_ARRAY_ITER_STOP;
            }
            break;
        case DATA_TEMPLATE_TYPE_ARRAY:
            property->value.value_string.str = TCI_HAL_Malloc(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_LEN + 1);
            if (!property->value.value_string.str) {
                Log_e("json get array buff failed");
                return UTILS_JSON_ARRAY_ITER_STOP;
            }
            property->value.value_string.str_len = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_LEN;
            break;
        case DATA_TEMPLATE_TYPE_STRUCT: {
            DataTemplateProperty parent_property_struct;
            memset(&parent_property_struct, 0, sizeof(parent_property_struct));
            parent_property_struct.type  = property->type;
            parent_property_struct.is_rw = property->is_rw;
            ret                          = _data_template_console_json_get_property(&parent_property_struct, params);
            if (ret != QCLOUD_RET_SUCCESS || !parent_property_struct.value.value_struct.count) {
                Log_e("json get struct member failed, %d, count: %d", ret,
                      parent_property_struct.value.value_struct.count);
                return UTILS_JSON_ARRAY_ITER_STOP;
            }
            property->value.value_struct.property = parent_property_struct.value.value_struct.property;
            property->value.value_struct.count    = parent_property_struct.value.value_struct.count;
        } break;
        default:
            break;
    }

    property->is_change = property->need_report = 0;
    property->change_ack = property->report_ack = 1;
    parent_property->value.value_struct.count++;
    return UTILS_JSON_ARRAY_ITER_CONTINUE;
}

static UtilsJsonArrayIterResult _data_template_console_json_array_property_count_callback(UtilsJsonValue json,
                                                                                          void*          user_data)
{
    DataTemplateProperty* parent_property = (DataTemplateProperty*)user_data;
    parent_property->value.value_struct.count++;
    return UTILS_JSON_ARRAY_ITER_CONTINUE;
}

static int _data_template_console_json_get_property(DataTemplateProperty* parent_property, UtilsJsonValue params)
{
    char* key     = DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS;
    int   key_len = sizeof(DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS) - 1;

    if (parent_property->type == DATA_TEMPLATE_TYPE_STRUCT) {
        key     = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS;
        key_len = sizeof(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS) - 1;

        int ret = utils_json_value_get(key, key_len, params.value, params.value_len, &params);
        if (ret) {
            /*  */
            Log_e("%s json get array buff failed %d", key, ret);
            return QCLOUD_ERR_FAILURE;
        }
    }

    /* get count */
    utils_json_array_parse(params.value, params.value_len, _data_template_console_json_array_property_count_callback,
                           parent_property);

    /* property count is zero */
    int count = parent_property->value.value_struct.count;
    if (!count) {
        Log_e("property count is 0");
        return QCLOUD_ERR_FAILURE;
    }

    /* malloc */
    parent_property->value.value_struct.property = TCI_HAL_Malloc(sizeof(DataTemplateProperty) * count);
    if (!parent_property->value.value_struct.property) {
        Log_e("property malloc failed size : %d", sizeof(DataTemplateProperty) * count);
        return QCLOUD_ERR_MALLOC;
    }

    memset(parent_property->value.value_struct.property, 0, sizeof(DataTemplateProperty) * count);

    /* parse json */
    parent_property->value.value_struct.count = 0;
    utils_json_array_parse(params.value, params.value_len, _data_template_console_json_array_property_callback,
                           parent_property);
    return QCLOUD_RET_SUCCESS;
}

static void _data_template_console_json_free_property(DataTemplateProperty* parent_property)
{
    if (!parent_property || !(parent_property->value.value_struct.property)) {
        return;
    }

    for (int i = 0; i < parent_property->value.value_struct.count; i++) {
        DataTemplateProperty* property = parent_property->value.value_struct.property + i;
        TCI_HAL_Free((void*)property->key);
        if (property->type == DATA_TEMPLATE_TYPE_STRING || property->type == DATA_TEMPLATE_TYPE_ARRAY) {
            TCI_HAL_Free(property->value.value_string.str);
            property->value.value_string.str_len = 0;
        } else if (property->type == DATA_TEMPLATE_TYPE_STRUCT) {
            DataTemplateProperty parent_property_struct;
            memset(&parent_property_struct, 0, sizeof(parent_property_struct));
            parent_property_struct.value.value_struct.property = property->value.value_struct.property;
            parent_property_struct.value.value_struct.count    = property->value.value_struct.count;
            _data_template_console_json_free_property(&parent_property_struct);
            TCI_HAL_Free(property->value.value_struct.property);
        }
    }

    TCI_HAL_Free(parent_property->value.value_struct.property);
    parent_property->value.value_struct.property = NULL;
    parent_property->value.value_struct.count    = 0;
}

static void _data_template_property_foreach(DataTemplate* data_template)
{
    if (!data_template || !(data_template->property)) {
        return;
    }
#if 0
    char* type_str[] = {"DATA_TEMPLATE_TYPE_BOOL",   "DATA_TEMPLATE_TYPE_INT",   "DATA_TEMPLATE_TYPE_STRING",
                        "DATA_TEMPLATE_TYPE_FLOAT",  "DATA_TEMPLATE_TYPE_ENUM",  "DATA_TEMPLATE_TYPE_TIME",
                        "DATA_TEMPLATE_TYPE_STRUCT", "DATA_TEMPLATE_TYPE_ARRAY", "DATA_TEMPLATE_TYPE_STRING_ENUM"};
#endif
    for (int i = 0; i < data_template->property_count; i++) {
        DataTemplateProperty* property = data_template->property + i;
#if 0
        if (property->type == DATA_TEMPLATE_TYPE_STRING || property->type == DATA_TEMPLATE_TYPE_ARRAY ||
            property->type == DATA_TEMPLATE_TYPE_STRING_ENUM) {
            Log_i("property[%d].value.value_int = %p;", i, property->value.value_string);
        } else if (property->type == DATA_TEMPLATE_TYPE_STRUCT) {
            Log_i("property[%d].value.value_int = %d;", i, property->value.value_struct.count);
        } else {
            Log_i("property[%d].value.value_int = %d;", i, property->value.value_int);
        }

        Log_i("property[%d].key             = \"%s\";", i, property->key);
        Log_i("property[%d].type            = %s;", i, type_str[property->type]);
        Log_i("property[%d].need_report     = %d;", i, property->need_report);
        Log_i("property[%d].is_rw           = %d;", i, property->is_rw);
        Log_i("");
#endif
        if (property->type == DATA_TEMPLATE_TYPE_STRUCT) {
            DataTemplate property_struct;
            memset(&property_struct, 0, sizeof(property_struct));
            property_struct.property       = property->value.value_struct.property;
            property_struct.property_count = property->value.value_struct.count;
            _data_template_property_foreach(&property_struct);
        }
    }
}

/**
 * @brief Parse property from console data template json string.
 *
 * @param[in,out] data_template usr data template
 * @param[in] params params filed of control message
 */
int iot_data_template_console_json_parse_property(DataTemplate* data_template, UtilsJsonValue params)
{
    POINTER_SANITY_CHECK(data_template, QCLOUD_ERR_FAILURE);
    /* property */
    DataTemplateProperty property;
    memset(&property, 0, sizeof(DataTemplateProperty));
    int ret = _data_template_console_json_get_property(&property, params);
    if (ret != QCLOUD_RET_SUCCESS || !property.value.value_struct.count) {
        _data_template_console_json_free_property(&property);

        Log_e("property parse failed %d, count:%d", ret, property.value.value_struct.count);
        return QCLOUD_ERR_FAILURE;
    }

    data_template->property       = property.value.value_struct.property;
    data_template->property_count = property.value.value_struct.count;

    _data_template_property_foreach(data_template);

    return QCLOUD_RET_SUCCESS;
}

/**
 * @brief free property from console data template json string.
 *
 * @param[in,out] data_template usr data template
 */
void iot_data_template_console_json_free_property(DataTemplate* data_template)
{
    POINTER_SANITY_CHECK_RTN(data_template);
    DataTemplateProperty property;
    memset(&property, 0, sizeof(DataTemplateProperty));
    property.value.value_struct.property = data_template->property;
    property.value.value_struct.count    = data_template->property_count;
    _data_template_console_json_free_property(&property);
    data_template->property       = NULL;
    data_template->property_count = 0;
}
