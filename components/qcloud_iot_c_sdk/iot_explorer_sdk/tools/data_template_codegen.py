#!/usr/bin/python
# -*- coding: utf-8 -*-

import json
import sys
import os
import argparse
import glob

from sys import version_info

if version_info.major == 3:
    import importlib
    importlib.reload(sys)
elif version_info.major == 2:
    reload(sys)
    sys.setdefaultencoding("utf-8")

try:
    import simplejson as json
except:
    import json

# {"version":"1.0","properties":[{"id":"light_switch","name":"电灯开关","desc":"控制电灯开灭","required":true,"mode":"rw","define":{"type":"bool","mapping":{"0":"关","1":"开"}}},{"id":"color","name":"颜色","desc":"灯光颜色","mode":"rw","define":{"type":"enum","mapping":{"0":"Red","1":"Green","2":"Blue"}}},{"id":"brightness","name":"颜色","desc":"灯光颜色","mode":"rw","define":{"type":"int","unit":"%","unitDesc":"亮度百分比","min":"0","max":"100"}},{"id":"name","name":"灯位置名称","desc":"灯位置名称：书房、客厅等","mode":"rw","required":true,"define":{"type":"string","min":"0","max":"64"}}]}


class iot_property:
    """Parse iot property json node and get property code.

    Retrieves rows pertaining to the given keys from the Table instance
    represented by table_handle.  String keys will be UTF-8 encoded.

    Args:
    json_node:
        json node include property
    prefix:
        property, action, event.
    Raises:
        ValueError: invalid node
    """

    def __init__(self, json_node, prefix="property", node_define="define"):
        self.prefix = prefix
        self.id = json_node["id"]
        self.type = json_node[node_define]["type"]
        self.default_value = ""
        self.struct_property_list = []
        self.mode = 1  # rw mode
        try:
            # property type dict
            PROPERTY_TYPE_DICT = {
                # key          type_id                            type_value_name
                'bool': ["DATA_TEMPLATE_TYPE_BOOL", "value_byte"],
                'enum': ["DATA_TEMPLATE_TYPE_ENUM", "value_uint"],
                'float': ["DATA_TEMPLATE_TYPE_FLOAT", "value_float"],
                'int': ["DATA_TEMPLATE_TYPE_INT", "value_int"],
                'string': ["DATA_TEMPLATE_TYPE_STRING", "value_string"],
                'timestamp': ["DATA_TEMPLATE_TYPE_TIME", "value_uint"],
                'struct': ["DATA_TEMPLATE_TYPE_STRUCT", "value_struct"],
                'stringenum': ["DATA_TEMPLATE_TYPE_STRING_ENUM", "value_string"],
                # 'array'     : ["DATA_TEMPLATE_TYPE_ARRAY"       , "value_array"       ], # TODO
            }
            self.type_id = PROPERTY_TYPE_DICT[self.type][0]
            self.type_value_name = PROPERTY_TYPE_DICT[self.type][1]
            # mode: rw or r
            if "mode" in json_node:
                self.mode = 1 if json_node["mode"] == "rw" else 0
            # default value
            if "mapping" in json_node[node_define]:
                self.default_value = next(
                    iter(json_node[node_define]["mapping"]))
            elif "start" in json_node[node_define]:
                self.default_value = json_node[node_define]["start"]
            elif "min" in json_node[node_define]:
                self.default_value = json_node[node_define]["min"]
            # string
            if self.type_id is "DATA_TEMPLATE_TYPE_STRING":
                self.max_length = json_node[node_define]["max"]
                self.default_value = "\" \""
            if self.type_id is "DATA_TEMPLATE_TYPE_STRING_ENUM":
                self.max_length = 0
                for enum in json_node[node_define]["mapping"]:
                    self.max_length = max(self.max_length, len(enum))
                self.default_value = "\"{}\"".format(
                    next(iter(json_node[node_define]["mapping"])))

            # struct
            if "specs" in json_node[node_define]:
                for subnode in json_node[node_define]["specs"]:
                    self.struct_property_list.append(iot_property(
                        subnode, self.prefix + '_' + self.id, "dataType"))
        except KeyError:
            ValueError('{} 字段 数据类型 type={} 取值非法，有效值应为：bool,enum,float,int,,string,timestamp,struct,stringenum'.format(
                self.id, self.type))

    def get_property_enum_index(self):
        return "USR_{}_INDEX_{}".format(self.prefix.upper(), self.id.upper())

    def create_property_struct_index_enum(self):
        main_code = ""
        main_code += "/*----------------- {} {} index enum start  -------------------*/\n\n".format(
            self.id, self.prefix)
        main_code += "typedef enum {\n"
        # 构造枚举索引
        for property in self.struct_property_list:
            if self.struct_property_list.index(property) is 0:
                main_code += "    {} = 0,\n".format(
                    property.get_property_enum_index())
            else:
                main_code += "    {},\n".format(
                    property.get_property_enum_index())

        main_code += "} "
        main_code += "Usr{}{}Index;\n\n".format(
            self.prefix.capitalize(), self.id.capitalize()).replace('_', '')
        main_code += "/*----------------- {} {} index enum end  -------------------*/\n\n".format(
            self.id, self.prefix)
        return main_code

    def get_property_struct_count(self):
        if self.type_id != "DATA_TEMPLATE_TYPE_STRUCT":
            return ""
        return "TOTAL_USR_{}_{}_COUNT".format(self.prefix.upper(), self.id.upper())

    def get_property_struct_init_func(self):
        if self.type_id != "DATA_TEMPLATE_TYPE_STRUCT":
            return ""
        return "_init_data_template_{}_{}".format(self.prefix, self.id)

    def get_property_struct_data_define(self):
        if self.type_id != "DATA_TEMPLATE_TYPE_STRUCT":
            return ""
        return "{}{}DataDefine".format(self.prefix.capitalize(), self.id.capitalize()).replace('_', '')

    def create_property_data_define(property_list):
        data_define = ""
        for property in property_list:
            if property.type_id is "DATA_TEMPLATE_TYPE_STRUCT":
                data_define += "    DataTemplateProperty {}[{}];\n".format(
                    property.id, property.get_property_struct_count())
                data_define += "    {} {}_data;\n".format(
                    property.get_property_struct_data_define(), property.id)
            if property.type_id in ["DATA_TEMPLATE_TYPE_STRING", "DATA_TEMPLATE_TYPE_STRING_ENUM"]:
                data_define += "    char {}[{}+1];\n".format(
                    property.id, property.max_length)
        return data_define

    def init_property_array(property_list):
        property_code = ""
        for property in property_list:
            property_array_index = "property[{}]".format(
                property.get_property_enum_index())
            if property.type_id is "DATA_TEMPLATE_TYPE_STRUCT":
                property_code += "    {}(data->{}, &data->{}_data);\n".format(
                    property.get_property_struct_init_func(), property.id, property.id)
                property_code += "    {}.value.{}.property = data->{};\n".format(
                    property_array_index, property.type_value_name, property.id)
                property_code += "    {}.value.{}.count = {};\n".format(
                    property_array_index, property.type_value_name, property.get_property_struct_count())
            elif property.type_id in ["DATA_TEMPLATE_TYPE_STRING", "DATA_TEMPLATE_TYPE_STRING_ENUM"]:
                property_code += "    {}.value.{}.str = data->{};\n".format(
                    property_array_index, property.type_value_name, property.id)
                property_code += "    {}.value.{}.str_len = strlen(data->{});\n".format(
                    property_array_index, property.type_value_name, property.id)
            else:
                property_code += "    {}.value.{} = {};\n".format(
                    property_array_index, property.type_value_name, property.default_value)
            property_code += "    {}.key = \"{}\";\n".format(
                property_array_index, property.id)
            property_code += "    {}.type = {};\n".format(
                property_array_index, property.type_id)
            property_code += "    {}.need_report = 1;\n".format(
                property_array_index)
            property_code += "    {}.is_rw = {};\n\n".format(
                property_array_index, property.mode)
        return property_code

    def create_property_struct_definition(property_list):
        main_code = ""
        for property in property_list:
            if property.type_id != "DATA_TEMPLATE_TYPE_STRUCT":
                continue
            # 定义结构体成员数目
            main_code += "#define {} {}\n\n".format(
                property.get_property_struct_count(), len(property.struct_property_list))
            main_code += "typedef struct {\n"
            main_code += iot_property.create_property_data_define(
                property.struct_property_list)
            main_code += "}} {};\n\n".format(
                property.get_property_struct_data_define())
            # 定义结构体类型和结构体变量
            main_code += "static void {}(DataTemplateProperty* property, {}* data)\n".format(
                property.get_property_struct_init_func(), property.get_property_struct_data_define())
            main_code += "{\n"
            main_code += iot_property.init_property_array(
                property.struct_property_list)
            main_code += "}\n\n"
        return main_code


class iot_event:
    def __init__(self, json_node):
        self.event_id = json_node["id"]
        self.event_type = json_node["type"]
        self.event_type_enum = "IOT_DATA_TEMPLATE_EVENT_TYPE_{}".format(
            self.event_type.upper())
        self.event_properties = []

        for property in json_node["params"]:
            self.event_properties.append(iot_property(property, "event"))

    def get_event_enum_index(self):
        return "USR_EVENT_INDEX_{}".format(self.event_id.upper())

    def create_event_params_global_var(self):
        main_code = ""
        main_code += "/**\n * @brief Sample of event {} post params.\n *\n */\n".format(
            self.event_id)
        main_code += "static const char* sg_usr_event_{}_params  = ".format(
            self.event_id)
        main_code += "\"{"

        for property in self.event_properties:
            if self.event_properties.index(property) > 0:
                main_code += ","
            if property.type_id in ["DATA_TEMPLATE_TYPE_STRING", "DATA_TEMPLATE_TYPE_STRING_ENUM"]:
                main_code += "\\\"{}\\\":{}".format(
                    property.id, property.default_value.replace('\"', '\\\"'))
            else:
                main_code += "\\\"{}\\\":{}".format(
                    property.id, property.default_value)

        main_code += "}\";\n\n"
        return main_code


class iot_action(iot_property):
    def __init__(self, json_node):
        self.action_id = json_node["id"]

        self.action_input = []
        for input in json_node["input"]:
            self.action_input.append(iot_property(
                input, 'action_' + self.action_id + '_input'))

        self.action_output = []
        for output in json_node["output"]:
            self.action_output.append(iot_property(
                output, 'action_' + self.action_id + '_output'))

        # 将input当做结构体属性，继承结构体属性的成员
        self.id = self.action_id
        self.prefix = 'action'
        self.struct_property_list = self.action_input
        self.type_id = "DATA_TEMPLATE_TYPE_STRUCT"

    def get_action_enum_index(self):
        return self.get_property_enum_index()

    def get_action_input_index_enum(self):
        return self.create_property_struct_index_enum()

    def get_action_input_init_func(self):
        return self.get_property_struct_init_func()

    def get_action_count(self):
        return self.get_property_struct_count()

    def get_action_data_define(self):
        return self.get_property_struct_data_define()

    def create_action_input_init(self):
        property_list = []
        property_list.append(self)
        return iot_property.create_property_struct_definition(property_list)

    def create_action_reply(self):
        main_code = ""
        input_struct = ""

        main_code += "/**\n * @brief Sample of action {} reply.\n *\n */\n".format(
            self.action_id)
        main_code += "static IotDataTemplateActionReply sg_usr_action_{}_reply = ".format(
            self.action_id)
        main_code += "{\n"
        main_code += "    .code = 0,\n"
        main_code += "    .client_token = {"
        main_code += ".value = \"test_{}\", .value_len = sizeof(\"test_{}\")".format(
            self.action_id, self.action_id)
        main_code += "},\n"
        main_code += "    .response = \"{"

        for property in self.action_output:
            if self.action_output.index(property) > 0:
                main_code += ","
            if property.type_id == "DATA_TEMPLATE_TYPE_STRUCT":
                # "position":{"longitude":30,"latitude":30}
                main_code += "\\\"{}\\\":{".format(property.id)
                for struct_property in self.struct_property_list:
                    if self.struct_property_list.index(struct_property) > 0:
                        main_code += ","
                    main_code += "\\\"{}\\\":{}".format(
                        struct_property.id, struct_property.default_value)
                main_code += "}"
            main_code += "\\\"{}\\\":{}".format(property.id,
                                                property.default_value)
        main_code += "}\",\n};\n\n"
        # print(main_code)
        return main_code


class iot_struct:
    def __init__(self, model):
        self.properties = []
        self.events = []
        self.actions = []

        if "properties" in model:
            for property in model["properties"]:
                self.properties.append(iot_property(property))

        if "events" in model:
            for event in model["events"]:
                self.events.append(iot_event(event))

        if "actions" in model:
            for action in model["actions"]:
                self.actions.append(iot_action(action))

    def __index_enum_create(self, list, prefix):
        property_struct_index_enum_str = ""
        action_input_index_enum_str = ""

        main_code = ""
        main_code += "/*----------------- {} index enum start  -------------------*/\n\n".format(
            prefix)
        main_code += "typedef enum {\n"

        enum_index = ""
        for node in list:
            if prefix is "property":
                enum_index = node.get_property_enum_index()
                if node.type_id is "DATA_TEMPLATE_TYPE_STRUCT":
                    property_struct_index_enum_str += node.create_property_struct_index_enum()
            elif prefix is "action":
                enum_index = node.get_action_enum_index()
                action_input_index_enum_str += node.get_action_input_index_enum()
            elif prefix is "event":
                enum_index = node.get_event_enum_index()

            if list.index(node) is 0:
                main_code += "    {} = 0,\n".format(enum_index)
            else:
                main_code += "    {},\n".format(enum_index)
        if len(list) is 0:
            main_code += "    USR_{}_INDEX_NONE = 0,\n".format(prefix.upper())

        main_code += "} "
        main_code += "Usr{}Index;\n\n".format(prefix.capitalize())
        main_code += "/*----------------- {} index enum end  -------------------*/\n\n".format(
            prefix)

        main_code += property_struct_index_enum_str
        main_code += action_input_index_enum_str
        return main_code

    def __property_data_initializer(self):
        main_code = ""
        main_code += "// ----------------------------------------------------------------------------\n"
        main_code += "// user property\n"
        main_code += "// ----------------------------------------------------------------------------/\n\n"
        # struct definition
        main_code += iot_property.create_property_struct_definition(
            self.properties)
        # data definition
        main_code += "#define TOTAL_USR_PROPERTY_COUNT {}\n\n".format(
            len(self.properties))
        main_code += "typedef struct {\n"
        main_code += iot_property.create_property_data_define(self.properties)
        main_code += "} PropertyDataDefine;\n\n"
        # data init
        main_code += "static void _init_data_template_property(DataTemplateProperty* property, PropertyDataDefine* data)\n"
        main_code += "{\n"
        main_code += iot_property.init_property_array(self.properties)
        main_code += "}\n\n"
        return main_code

    def __event_data_initializer(self):
        main_code = ""
        main_code += "// ----------------------------------------------------------------------------\n"
        main_code += "// user event\n"
        main_code += "// ----------------------------------------------------------------------------\n\n"

        event_code = ""
        event_code += "#define TOTAL_USR_EVENT_COUNT {}\n\n".format(
            len(self.events))
        event_code += "static void _init_data_template_event(DataTemplateEvent* event)\n"
        event_code += "{\n"

        for event in self.events:
            main_code += event.create_event_params_global_var()
            static_usr_event_array_name = "event[{}]".format(
                event.get_event_enum_index())
            event_code += "    {}.event_id = \"{}\";\n".format(
                static_usr_event_array_name, event.event_id)
            event_code += "    {}.type = {};\n".format(
                static_usr_event_array_name, event.event_type_enum)
            event_code += "    {}.params = sg_usr_event_{}_params;\n\n".format(
                static_usr_event_array_name, event.event_id)
        event_code += "}\n\n"

        main_code += event_code
        # print(event_params)
        return main_code

    def __action_data_initializer(self):
        main_code = ""
        action_code = ""

        main_code += "// ----------------------------------------------------------------------------\n"
        main_code += "// user action\n"
        main_code += "// ----------------------------------------------------------------------------\n\n"
        # count
        action_code += "#define TOTAL_USR_ACTION_COUNT {}\n\n".format(
            len(self.actions))
        # data definition
        action_code += "typedef struct {\n"
        action_code += iot_property.create_property_data_define(self.actions)
        action_code += "} ActionDataDefine;\n\n"
        action_code += "static void _init_data_template_action(DataTemplateAction* action, ActionDataDefine* data)\n"
        action_code += "{\n"
        for action in self.actions:
            main_code += action.create_action_input_init()
            main_code += action.create_action_reply()
            static_usr_action_input_array_name = "action[{}]".format(
                action.get_action_enum_index())
            action_code += "    {}(data->{},&data->{}_data);\n".format(
                action.get_action_input_init_func(), action.action_id, action.action_id)
            action_code += "    {}.action_id = \"{}\";\n".format(
                static_usr_action_input_array_name, action.action_id)
            action_code += "    {}.input_struct.value_struct.property = data->{};\n".format(
                static_usr_action_input_array_name, action.action_id)
            action_code += "    {}.input_struct.value_struct.count = {};\n".format(
                static_usr_action_input_array_name, action.get_action_count())
            action_code += "    {}.reply = sg_usr_action_{}_reply;\n\n".format(
                static_usr_action_input_array_name, action.action_id)
        action_code += "}\n\n"

        main_code += action_code
        return main_code

    def __usr_api_initializer(self):
        main_code = "// ----------------------------------------------------------------------------\n"
        main_code += "// user data template create\n"
        main_code += "// ----------------------------------------------------------------------------\n\n"
        main_code += "static DataTemplate* _usr_data_template_create(void)\n"
        main_code += "{\n"
        main_code += "    DataTemplate* data_template = iot_data_template_create(TOTAL_USR_PROPERTY_COUNT, sizeof(PropertyDataDefine), TOTAL_USR_EVENT_COUNT, TOTAL_USR_ACTION_COUNT, sizeof(ActionDataDefine));\n"
        main_code += "    if (!data_template) {\n"
        main_code += "        return NULL;\n"
        main_code += "    }\n"
        main_code += "    _init_data_template_property(data_template->property, data_template->property_data);\n"
        main_code += "    _init_data_template_event(data_template->event);\n"
        main_code += "    _init_data_template_action(data_template->action, data_template->action_data);\n"
        main_code += "    return data_template;\n"
        main_code += "}\n"
        return main_code

    def gen_config_header(self):
        main_code = "#include \"qcloud_iot_data_template_config.h\"\n\n"
        main_code += self.__index_enum_create(self.properties, "property")
        main_code += self.__index_enum_create(self.events, "event")
        main_code += self.__index_enum_create(self.actions, "action")
        return main_code

    def gen_config_src_c(self):
        main_code = ""
        main_code += self.__property_data_initializer()
        main_code += self.__event_data_initializer()
        main_code += self.__action_data_initializer()
        main_code += self.__usr_api_initializer()
        return main_code


def main():
    parser = argparse.ArgumentParser(description='Iot hub data_template and events config code generator.',
                                     usage='use "./data_template_codegen.py -c xx/config.json" gen config code')
    parser.add_argument('-c', '--config', dest='config', metavar='xxx.json', required=False, default='xxx.json',
                        help='copy the generated file (data_template_config.include) to data_template_sample dir '
                             'or your own code dir with data_template. '
                        '\nconfig file can be download from tencent iot-explorer platform. https://console.cloud.tencent.com/iotexplorer')
    parser.add_argument('-d', '--dest', dest='dest', required=False, default='./data_template_config.include',
                        help='Dest directory for generated code files, no / at the end.')
    args = parser.parse_args()

    config_path = args.config
    if not os.path.exists(config_path):
        print(u"错误：配置文件不存在，请重新指定数据模板配置文件路径,请参考用法 ./data_template_code_generate.py -c <dir>/data_template.json".format(config_path))
        return 1

    config_dir = os.path.dirname(config_path)
    if config_dir:
        config_dir += "/"

    f = open(config_path, "r", encoding='utf-8')
    try:
        thing_model = json.load(f)
        if 'properties' not in thing_model:
            thing_model.properties = []

        if 'events' not in thing_model:
            thing_model.events = []

        print(u"加载 {} 文件成功".format(config_path))
    except ValueError as e:
        print(u"错误：文件格式非法，请检查 {} 文件是否是 JSON 格式。".format(config_path))
        return 1

    try:
        snippet = iot_struct(thing_model)
        output_config_header_file_name = args.dest
        with open(output_config_header_file_name, "w") as file:
            file.write("{}".format(snippet.gen_config_header()))
            file.write("{}".format(snippet.gen_config_src_c()))
            file.close()
            print(u"文件 {} 生成成功".format(output_config_header_file_name))
        return 0
    except ValueError as e:
        print(e)
        return 1


if __name__ == '__main__':
    sys.exit(main())
