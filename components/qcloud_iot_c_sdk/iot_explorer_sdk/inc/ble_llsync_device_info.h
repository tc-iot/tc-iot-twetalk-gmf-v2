/**
 * @file ble_llsync_device_info.h
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2022-12-01
 *
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
 * @par Change Log:
 * <table>
 * Date				Version		Author			Description
 * 2022-12-01		1.0			hubertxxu		first commit
 * </table>
 */
#ifndef IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_DEVICE_INFO_H_
#define IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_DEVICE_INFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_error.h"
#include "utils_hmac.h"

#define LLSYNC_DYNREG_MASK_BIT       0x02
#define LLSYNC_BIND_STATE_MASK       0x03
#define LLSYNC_PROTO_VER_BIT         0x04
#define LLSYNC_PROTOCOL_VERSION_MASK 0xF0
#define LLSYNC_MTU_SET_MASK          0x8000

#define LLSYNC_MTU_SET_RESULT_ERR 0xFFFF  // some error when setting mtu

#define BLE_QIOT_LLSYNC_PROTOCOL_VERSION (4)  // llsync protocol version, equal or less than 15

#define ATT_DEFAULT_MTU                 23  // default att mtu
#define ATT_MTU_TO_LLSYNC_MTU(_att_mtu) ((_att_mtu)-3)

#define BLE_LOCAL_PSK_LEN           4
#define BLE_BIND_IDENTIFY_STR_LEN   8
#define BLE_EXPIRATION_TIME         60  // timestamp expiration value
#define BLE_UNBIND_REQUEST_STR      "UnbindRequest"
#define BLE_UNBIND_REQUEST_STR_LEN  (sizeof("UnbindRequest") - 1)
#define BLE_UNBIND_RESPONSE         "UnbindResponse"
#define BLE_UNBIND_RESPONSE_STR_LEN (sizeof("UnbindResponse") - 1)

typedef enum {
    E_DEV_MSG_SYNC_TIME = 0,  // sync info before bind
    E_DEV_MSG_CONN_VALID,     // connect request
    E_DEV_MSG_BIND_SUCC,      // inform bind success
    E_DEV_MSG_BIND_FAIL,      // inform bind failed
    E_DEV_MSG_UNBIND,         // unbind request
    E_DEV_MSG_CONN_SUCC,      // inform connect result
    E_DEV_MSG_CONN_FAIL,
    E_DEV_MSG_UNBIND_SUCC,  // inform unbind result
    E_DEV_MSG_UNBIND_FAIL,
    E_DEV_MSG_SET_MTU_RESULT,  // inform set mtu result
    E_DEV_MSG_BIND_TIMEOUT,    // inform bind timeout
    E_DEV_MSG_DYNREG,
    E_DEV_MSG_GET_DEV_INFO = 0xE0,  // configure network start
    E_DEV_MSG_SET_WIFI_MODE,
    E_DEV_MSG_SET_WIFI_INFO,
    E_DEV_MSG_SET_WIFI_CONNECT,
    E_DEV_MSG_SET_WIFI_TOKEN,
    E_DEV_MSG_GET_DEV_LOG,
    E_DEV_MSG_MSG_BUTT,
} BLELLsyncDeviceInfoType;

typedef enum {
    E_LLSYNC_BIND_IDLE = 0,  // no bind
    E_LLSYNC_BIND_WAIT,      // wait bind, return idle state if no bind in the period
    E_LLSYNC_BIND_SUCC,      // bound
} BLELLsyncBindState;

typedef enum {
    E_LLSYNC_DISCONNECTED = 0,
    E_LLSYNC_CONNECTED,
} BLELLsyncConnectState;

typedef enum {
    E_BLE_DISCONNECTED = 0,
    E_BLE_CONNECTED,
} BLEConnectState;

typedef struct ble_core_data_ {
    uint8_t bind_state;
    char    local_psk[BLE_LOCAL_PSK_LEN];
    char    identify_str[BLE_BIND_IDENTIFY_STR_LEN];
} BLELLsyncCoreData;

// write to uuid FEE1 before bind
typedef struct {
    int nonce;
    int timestamp;
} BLELLsyncBindData;

// connect data struct
typedef struct {
    int  timestamp;
    char sign_info[SHA1_DIGEST_SIZE];
} BLELLsyncConnectData;

// unbind data struct
typedef struct {
    char sign_info[SHA1_DIGEST_SIZE];
} BLELLsyncUnbindData;

typedef struct {
    IotBool  have_data;  // start received package
    uint8_t  type;       // event type
    uint16_t buf_len;    // the length of data
    char     buf[BLE_QIOT_EVENT_MAX_SIZE];
} BLELLsyncEventSlice;

int llsync_data_init(uint8_t mac[6], DeviceInfo *dev_info);

void llsync_bind_state_set(BLELLsyncBindState new_state);

BLELLsyncBindState llsync_bind_state_get(void);

void llsync_connection_state_set(BLELLsyncConnectState new_state);

void ble_connection_state_set(BLEConnectState new_state);

IotBool llsync_is_connected(void);

IotBool ble_is_connected(void);

int ble_get_my_broadcast_data(char *out_buf, int buf_len);

int ble_bind_get_authcode(const char *bind_data, uint16_t data_len, char *out_buf, uint16_t buf_len);

int ble_dynreg_get_authcode(const char *bind_data, uint16_t data_len, char *out_buf, uint16_t buf_len);

int ble_dynreg_parse_psk(const char *in_buf, uint16_t data_len);

int ble_bind_write_result(const char *result, uint16_t len);

int ble_unbind_write_result(void);

int ble_conn_get_authcode(const char *conn_data, uint16_t data_len, char *out_buf, uint16_t buf_len);

int ble_unbind_get_authcode(const char *unbind_data, uint16_t data_len, char *out_buf, uint16_t buf_len);

int ble_inform_mtu_result(const char *result, uint16_t data_len);

uint16_t llsync_mtu_get(void);

void llsync_mtu_update(uint16_t sync_mtu);

IotBool is_llsync_need_dynreg(void);

int ble_get_device_name(char *output_name);

int ble_get_device_version(char *output_version);

#ifdef __cplusplus
}
#endif
#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_DEVICE_INFO_H_
