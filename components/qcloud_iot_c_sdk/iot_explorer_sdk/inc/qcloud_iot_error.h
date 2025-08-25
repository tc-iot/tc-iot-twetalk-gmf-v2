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
 * @file qcloud_iot_error.h
 * @brief error code of sdk
 * @author fancyxu (fancyxu@tencent.com)
 * @version 1.0
 * @date 2021-05-28
 *
 * @par Change Log:
 * <table>
 * <tr><th>Date       <th>Version <th>Author    <th>Description
 * <tr><td>2021-05-28 <td>1.0     <td>fancyxu   <td>first commit
 * <tr><td>2021-07-08 <td>1.1     <td>fancyxu   <td>fix code standard of IotReturnCode and QcloudIotClient
 * </table>
 */

#ifndef IOT_HUB_DEVICE_C_SDK_INCLUDE_COMMON_QCLOUD_IOT_ERROR_H_
#define IOT_HUB_DEVICE_C_SDK_INCLUDE_COMMON_QCLOUD_IOT_ERROR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "tc_iot_ret_code.h"

/**
 * @brief IOT SDK bool type
 *
 */
#define IotBool uint8_t

/**
 * @brief IOT bool false
 *
 */
#define IOT_BOOL_FALSE 0x00

/**
 * @brief IOT bool true
 *
 */
#define IOT_BOOL_TRUE 0x01

/**
 * @brief IOT SDK return/error code.
 * Enumeration of return code in QCloud IoT C-SDK.
 * Values less than 0 are specific error codes
 * Value of 0 is successful return
 * Values greater than 0 are specific non-error return codes
 *
 */
typedef enum {

    QCLOUD_RET_MQTT_ALREADY_CONNECTED           = 4, /**< Already connected with MQTT server */
    QCLOUD_RET_MQTT_CONNACK_CONNECTION_ACCEPTED = 3, /**< MQTT connection accepted by server */
    QCLOUD_RET_MQTT_MANUALLY_DISCONNECTED       = 2, /**< Manually disconnected with MQTT server */
    QCLOUD_RET_MQTT_RECONNECTED                 = 1, /**< Reconnected with MQTT server successfully */

    QCLOUD_ERR_MQTT_NO_CONN              = -101, /**< Not connected with MQTT server */
    QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT = -102, /**< Reconnecting with MQTT server */
    QCLOUD_ERR_MQTT_RECONNECT_TIMEOUT    = -103, /**< MQTT reconnect timeout */
    QCLOUD_ERR_MQTT_SUB                  = -104, /**< MQTT topic subscription fail */
    QCLOUD_ERR_MQTT_NOTHING_TO_READ      = -105, /**< MQTT nothing to read */
    QCLOUD_ERR_MQTT_PACKET_READ          = -106, /**< Something wrong when reading MQTT packet */
    QCLOUD_ERR_MQTT_REQUEST_TIMEOUT      = -107, /**< MQTT request timeout */
    QCLOUD_ERR_RX_MESSAGE_INVAL          = -108, /**< MQTT received invalid msg */
    QCLOUD_ERR_BUF_TOO_SHORT             = -109, /**< MQTT recv buffer not enough */
    QCLOUD_ERR_MQTT_QOS_NOT_SUPPORT      = -110, /**< MQTT QoS level not supported */
    QCLOUD_ERR_MQTT_UNSUB_FAIL           = -111, /**< MQTT unsubscribe failed */
    QCLOUD_ERR_MAX_TOPIC_LENGTH          = -112, /**< Topic length oversize */

    QCLOUD_ERR_HTTP           = -201, /**< HTTP unknown error */
    QCLOUD_ERR_HTTP_AUTH      = -202, /**< HTTP auth failed */
    QCLOUD_ERR_HTTP_NOT_FOUND = -203, /**< HTTP 404 */
    QCLOUD_ERR_HTTP_TIMEOUT   = -204, /**< HTTP timeout */
    QCLOUD_ERR_HTTP_PARSE     = -205, /**< HTTP URL parse failed */
    QCLOUD_ERR_HTTP_CONNECT   = -206, /**< HTTP connect failed */
    QCLOUD_ERR_HTTP_CLOSE     = -207, /**< HTTP connect close by server */

    QCLOUD_ERR_JSON_PARSE = -300, /**< JSON parsing error */

    QCLOUD_ERR_TLV_TIMEOUT         = -800, /**< TLV timeout*/
    QCLOUD_ERR_TLV_PARSE           = -801, /**< TLV timeout*/
    QCLOUD_ERR_TLV_SIGN            = -802, /**< TLV  sign error*/
    QCLOUD_ERR_TLV_SEND_FAIL       = -803, /**< TLV send fail*/
    QCLOUD_ERR_TLV_NO_EXIST        = -804, /**< TLV send fail*/
    QCLOUD_ERR_WIFI_CONFIG_TIMEOUT = -805, /**< wifi config timeout */

    QCLOUD_ERR_TWETALK_ALREADY_INIT         = -900, /**< twetalk already init */
    QCLOUD_ERR_TWETALK_CONNECT_FAIL         = -901, /**< twetalk connect fail */
    QCLOUD_ERR_TWETALK_CREATE_THREAD_FAIL   = -902, /**< twetalk create thread fail */
    QCLOUD_ERR_TWETALK_CTREATE_RINGBUF_FAIL = -903, /**< twetalk create ringbuf fail */
    QCLOUD_ERR_TWETALK_ALREADY_EXIT         = -904, /**< twetalk already exit */
    QCLOUD_ERR_TWETALK_NOT_INIT             = -905, /**< twetalk not init */
    QCLOUD_ERR_TWETALK_SEND_FAIL            = -906, /**< twetalk send fail */
    QCLOUD_ERR_TWETALK_MALLOC_FAIL          = -907, /**< twetalk malloc fail */
    QCLOUD_ERR_TWETALK_CREATE_SEM_FAIL      = -908, /**< twetalk create sem fail */
    QCLOUD_ERR_TWETALK_NOT_CONNECTED        = -909, /**< twetalk not connected */
    QCLOUD_ERR_TWETALK_UNKNOW_TYPE          = -910, /**< twetalk unknow type */

    QCLOUD_ERR_WS_CLOSE_NORMAL                     = -2000, /**< websocket close normal */
    QCLOUD_ERR_WS_CLOSE_GOING_AWAY                 = -2001, /**< websocket close going away */
    QCLOUD_ERR_WS_CLOSE_PROTOCOL_ERROR             = -2002, /**< websocket close protocol error */
    QCLOUD_ERR_WS_CLOSE_UNSUPPORTED_DATA           = -2003, /**< websocket close unsupported data */
    QCLOUD_ERR_WS_CLOSE_NO_STATUS_RECEIVED         = -2005, /**< websocket close no status received */
    QCLOUD_ERR_WS_CLOSE_ABNORMAL_CLOSURE           = -2006, /**< websocket close abnormal closure */
    QCLOUD_ERR_WS_CLOSE_INVALID_FRAME_PAYLOAD_DATA = -2007, /**< websocket close invalid frame payload data */
    QCLOUD_ERR_WS_CLOSE_POLICY_VIOLATION           = -2008, /**< websocket close policy violation */
    QCLOUD_ERR_WS_CLOSE_MESSAGE_TOO_BIG            = -2009, /**< websocket close message too big */
    QCLOUD_ERR_WS_CLOSE_MANDATORY_EXT              = -2010, /**< websocket close mandatory extension */
    QCLOUD_ERR_WS_CLOSE_INTERNAL_SERVER_ERROR      = -2011, /**< websocket close internal server error */
    QCLOUD_ERR_WS_CLOSE_TLS_HANDSHAKE              = -2015, /**< websocket close tls handshake */

} IotReturnCode;

#ifdef __cplusplus
}
#endif

#endif  // IOT_HUB_DEVICE_C_SDK_INCLUDE_COMMON_QCLOUD_IOT_ERROR_H_
