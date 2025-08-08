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
 * @file qcloud_iot_token_api.h
 * @brief
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2022-12-13
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2022-12-13 <td>1.0     <td>fancyxu   <td>first commit
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_COMMON_QCLOUD_IOT_TOKEN_API_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_COMMON_QCLOUD_IOT_TOKEN_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "qcloud_iot_common.h"

/**
 * @brief Http token api timeout.
 *
 */
#define HTTP_TOKEN_API_WAIT_MAX_TIMEOUT_MS (5000)

typedef enum {
    IOT_TOKEN_API_ACTION_GET_PRODUCT_INFO,            // https://cloud.tencent.com/document/product/1081/48764
    IOT_TOKEN_API_ACTION_CONTROL_DEVICE_DATA,         // https://cloud.tencent.com/document/product/1081/40805
    IOT_TOKEN_API_ACTION_GET_FAMILY_LIST,             // https://cloud.tencent.com/document/product/1081/40811
    IOT_TOKEN_API_ACTION_GET_ROOM_LIST,               // https://cloud.tencent.com/document/product/1081/40816
    IOT_TOKEN_API_ACTION_GET_DEVICE_LIST,             // https://cloud.tencent.com/document/product/1081/40803
    IOT_TOKEN_API_ACTION_GET_DEVICE_STATUS,           // https://cloud.tencent.com/document/product/1081/40804
    IOT_TOKEN_API_ACTION_GET_DEVICE_IN_FAMILY,        // https://cloud.tencent.com/document/product/1081/40807
    IOT_TOKEN_API_ACTION_GET_DEVICE_SUB_DEVICE_LIST,  // https://cloud.tencent.com/document/product/1081/48130
    IOT_TOKEN_API_ACTION_GET_SCENE_LIST,              // https://cloud.tencent.com/document/product/1081/50211
    IOT_TOKEN_API_ACTION_RUN_SCENE,                   // https://cloud.tencent.com/document/product/1081/50214
    IOT_TOKEN_API_ACTION_GET_AUTOMATION_LIST,         // https://cloud.tencent.com/document/product/1081/50216
    IOT_TOKEN_API_ACTION_MODIFY_AUTOMATION_STATUS,    // https://cloud.tencent.com/document/product/1081/50220
    IOT_TOKEN_API_ACTION_APP_SMART_HOME_DISCOVER,
    IOT_TOKEN_API_ACTION_SMART_HOME_DISCOVER,
    IOT_TOKEN_API_ACTION_GET_PRODUCT_CATEGORY,
} IotTokenApiAction;

/**
 * @brief Params using in request token api.
 *
 */
typedef union {
    struct {
        const char *products_array;
    } get_product_info;
    struct {
        const char *product_id;
        const char *device_name;
        const char *control_data;
    } control_device_data;
    struct {
        const char *device_array;
    } get_device_status;
    struct {
        const char *product_id;
        const char *device_name;
        const char *family_id;
    } get_device_in_family;
    struct {
        const char *gw_product_id;
        const char *gw_device_name;
    } get_device_sub_device_list;
    struct {
        const char *family_id;
        size_t      offset;
        size_t      limit;
    } get_scene_list;
    struct {
        const char *scene_id;
    } run_scene;
    struct {
        const char *automation_id;
        uint8_t     status;
    } modify_automation_status;
    struct {
        const char *platform_key;
    } get_product_category;
    struct {
        const char *product_id;
        const char *device_name;
        const char *platform_key;
    } smart_home_discover;

    struct {
        // IOT_TOKEN_API_ACTION_GET_FAMILY_LIST: NULL
        // IOT_TOKEN_API_ACTION_GET_ROOM_LIST: family_id
        // IOT_TOKEN_API_ACTION_GET_DEVICE_LIST: family_id
        // IOT_TOKEN_API_ACTION_GET_SCENE_LIST: family_id
        // IOT_TOKEN_API_ACTION_GET_AUTOMATION_LIST: family_id
        const char *family_id;
        size_t      family_id_len;
    } default_params;
} IotTokenApiActionParams;

/**
 * @brief Request token api & get response.
 *
 * @param[in] action @see IotTokenApiAction
 * @param[in] params @see IotTokenApiActionParams
 * @param[out] buf response buf
 * @param[in] buf_len buf length
 * @param[in] access_token access_token get from cloud
 * @return response length
 */
size_t IOT_TokenApi_Request(IotTokenApiAction action, IotTokenApiActionParams *params, char *buf, size_t buf_len,
                            const char *access_token);

// 1. IOT_TOKEN_API_ACTION_GET_PRODUCT_INFO
// req:
//   "ProductIds":["FVYYYEL4ON"]
// rep:
//   "Products":[
//   {
//       "ProductId":"FVYYYEL4ON",
//       "Name":"c1",
//       "Description":"yes",
//       "State":"",
//       "DataTemplate":"{"version":"1.0","profile":{"ProductId":"FVYYYEL4ON","CategoryId":"1"},"properties":[{"id":"mac","name":"mac地址","desc":"wifi路由器mac地址","required":true,"mode":"r","define":{"type":"string","min":"0","max":"30"}},{"id":"signal","name":"wifi强度","desc":"wifi信号强度值","mode":"r","define":{"type":"int","unit":"dbm","step":"1","min":"-180","max":"180","start":"1"}},{"id":"ssid","name":"wifi热点名","desc":"设备连接的wifi热点名","mode":"r","define":{"type":"string","min":"0","max":"64"}},{"id":"severip","name":"wifi路由器接入的ip","desc":"wifi路由器接入的ip","mode":"r","define":{"type":"string","min":"0","max":"64"}},{"id":"nmacs","name":"其他热点mac地址","desc":"可接受到的其他热点mac地址","mode":"r","define":{"type":"string","min":"0","max":"64"}}],"events":[],"actions":[]}",
//       "AppTemplate":"",
//       "NetType":"wifi",
//       "CategoryId":1,
//       "ProductType":0,
//       "UpdateTime":1600159203
//   }
// 2. IOT_TOKEN_API_ACTION_CONTROL_DEVICE_DATA:
// req:
//   "ProductId":"22F9Y6II7O","DeviceName":"light1","Data":"{\"light_switch\":0}"
// rep:
//   {"Data":"","RequestId":"req_1"}
// 3. IOT_TOKEN_API_ACTION_GET_FAMILY_LIST:
// rep:
//  "FamilyList":[{
//      "FamilyId":"a1c6939b39d345b897b168313f8ca12c",
//      "Name":"family_name",
//      "Role":0,  // 1:管理员  0：普通成员
//      "CreateTime":1570786578,
//      "UpdateTime":1570790807
//  }]
// 4. IOT_TOKEN_API_ACTION_GET_ROOM_LIST
// req:
//  "FamilyId":"f_9****c1"
// rep:
//  "FamilyList":[{
//      "RoomId":"123456",
//      "RoomName":"name",
//      "DeviceNum":2,
//      "CreateTime":1570786578,
//      "UpdateTime":1570790807
//   }]
// 5. IOT_TOKEN_API_ACTION_GET_DEVICE_LIST
// req:
//  "FamilyId":"e0e23a2b33a24652b606ef9107c9a1cf","RoomId" : "5",  // ""表示所有 "0"表示默认
// rep:
//  "DeviceList": [{
//      "ProductId": "R32ONVL0EU",
//      "DeviceName": "df2eSJyY",
//      "DeviceId": "R32ONVL0EU/df2eSJyY",
//      "AliasName": "df2eSJyY",
//      "UserID": "1",
//      "RoomId": "r_4b27c753ef774d458e0cf8b7686486ad",
//      "IconUrl": "",
//      "CreateTime": 1574664969,
//      "UpdateTime": 1574668779
//  }]
// 6. IOT_TOKEN_API_ACTION_GET_DEVICE_STATUS:
// req:
//  "DeviceIds": ["HY4DHFM5P6/81386276"]
// rep:
//  "DeviceStatuses":[{
//      "DeviceId":"22F9Y6II7O/light1",
//      "ProductId":"22F9Y6II7O",
//      "DeviceName":"light1",
//      "Online":0  // 0 在线；1：离线
//  }]
// 7. IOT_TOKEN_API_ACTION_GET_DEVICE_IN_FAMILY
// req:
//  "ProductId":"R32ONVL0EU","FamilyId":"xxx","DeviceName":"df2eSJyY"
// rep:
//  "Data" : {
//      "DeviceId" : "R32ONVL0EU/df2eSJyY",
//      "ProductId" : "R32ONVL0EU",
//      "DeviceName" : "df2eSJyY",
//      "AliasName" : "12345",
//      "IconUrl" : "",
//      "FamilyId" : "",
//      "RoomId" : "",
//      "DeviceType" : 0,  // 0 普通  1 网关设备  2 子设备
//      "CreateTime" : 1574931773,
//      "UpdateTime" : 1574931945
//  }
// 8. IOT_TOKEN_API_ACTION_GET_DEVICE_SUB_DEVICE_LIST
// req:
//  "GatewayProductId":"NJ27OVLZT4","GatewayDeviceName":"gwdev"
// rep:
//  "DeviceList": [{
//     "FamilyId": "f_9b309d84c962****0a11b4c3d9588fcc1",
//     "ProductId": "LAEG4YJE1A",
//     "DeviceName": "subdev1",
//     "DeviceId": "LAEG4YJE1A/subdev1",
//     "AliasName": "",
//     "RoomId": "0",
//     "IconUrl": "",
//     "DeviceType": 2,
//     "CreateTime": 1583492992,
//     "UpdateTime": 1583492992
// }]
// 9. IOT_TOKEN_API_ACTION_GET_SCENE_LIST
// req:
//  "FamilyId": "f_9b309********4c3d9588fcc1","Offset" : 0,"Limit" : 10
// rep:
//  "SceneList": [
//      {
//        "SceneId": "s_f7feb440d******28f3a598d00c",
//        "FamilyId": "f_9b309d8******a11b4c3d9588fcc1",
//        "SceneName": "f****na",
//        "SceneIcon": "",
//        "Actions": [         // Actions数组中的每项的字段，为空则不出现
//          {
//            "ActionType": 0,
//            "ProductId": "R32****0EU",
//            "DeviceName": "df****yY",
//            "Data": "{\"brightness\": 25}",
//            "AliasName": "1***45"      // 说明：AliasName为设备别名 。
//          },
//          {
//            "ActionType": 1,
//            "Data": "100"
//          }
//        ],
//        "UserId": "1",
//        "CreateTime": 1578557536,
//        "UpdateTime": 1578560598,
//        "Flag": 0,  // Flag ：0 为正常的场景，1：场景中的设备异常，不在家庭中
//        "Status": 0 // 场景运行状态 0未运行，1运行中
//      }
//  ]
// 10. IOT_TOKEN_API_ACTION_RUN_SCENE
// req:
//  "SceneId" : "s_527cb5*********53cb6a46f653"
// rep:
//  {"RequestId":"req_1"}
// 11. IOT_TOKEN_API_ACTION_GET_AUTOMATION_LIST:
// req:
//  "FamilyId":"f_9****c1"
// rep:
//  "List" : [ {
//      "AutomationId" : "a_cd61********d21d6aeffea",
//      "Icon" : "https://www.****.com/",
//      "Name" : "autoName",
//      "Status" : 0
//  } ]
// 12. IOT_TOKEN_API_ACTION_SMART_HOME_DISCOVER:
// req:
//  "FamilyId":"f_9****c1"
// rep:
//  "Data" : {
//      "DataTemplateList" : [ {
//          "ProductId" : "Q555ZLBNB",
//          "Properties" : [
//              {
//                  "define" : {"mapping" : {"0" : "关", "1" : "开"}, "type" : "bool"},
//                  "desc" : "控制电灯开灭",
//                  "id" : "power_switch",
//                  "mode" : "rw",
//                  "name" : "电灯开关",
//                  "required" : true
//              },
//              {
//                  "define" :{"max" : "100", "min" : "0", "start" : "1", "step" : "1", "type" : "int",
//                  "unit" :"%"}, "desc" : "灯光亮度", "id" : "brightness", "mode" : "rw", "name" : "亮度"
//              }
//          ],
//          "Version" : "1.0"
//      } ],
//      "FamilyList" : [ {
//          "FamilyId" : "34564567dfghdf",
//          "FamilyName" : "A家庭",
//          "Roomlist" : [ {
//              "deviceList" : [ {
//                  "AliasName" : "台灯",
//                  "DeviceData" : "{\"brightness\":{\"lastUpdate\":1552904784552,\"value\":8},\"color\":{"
//                                 "\"lastUpdate\":1552904784552,\"value\":1},\"light_switch\":{\"lastUpdate\":"
//                                 "1552904784552,\"value\":0}}",
//                  "DeviceId" : "R32ONVL0EU",
//                  "DeviceName" : "df2eSJyY",
//                  "DeviceType" : 0,
//                  "Status" : 0,
//                  "ProductId" : "DDFONVL0EU",
//                  "UserID" : "123"
//              } ],
//              "roomId" : "45645rt",
//              "roomName" : "kajshkrrw"
//          } ],
//          "SceneList" : [
//              {"SceneId" : "s_f7feb440d******28f3a598d00c", "SceneName" : "f****na", "Status" : 0},
//              {"SceneId" : "s_33333******28f3a598d00c", "SceneName" : "f*5566a", "Status" : 1}
//          ]
//      } ]
//  }
// 13. IOT_TOKEN_API_ACTION_MODIFY_AUTOMATION_STATUS:
// req:
//  "AutomationId": "a_cd61dd******1d6aeffea","Status": 1 //联动的开关 0：关闭 1：启用
// rep:
//  "RequestId":"req_1"
// 14.IOT_TOKEN_API_ACTION_GET_PRODUCT_CATEGORY:
// req:
//  "PlatformKey": "XF"
// rep:
//  "Data" : [
//      {"ProductId":"HJJWO7QKTT","Category":"light"}, {"ProductId":"PUEHCNDH1V","Category":""},
//      {"ProductId":"3ILDLBGJKK","Category":"airControl"},{"ProductId":"75E3J3MXCJ","Category":"light"}
//  ]

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_SERVICES_COMMON_QCLOUD_IOT_TOKEN_API_H_
