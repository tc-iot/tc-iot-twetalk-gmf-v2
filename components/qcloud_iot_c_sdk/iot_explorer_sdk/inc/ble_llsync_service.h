/**
 * @file ble_qiot_service.h
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
#ifndef IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_SERVICE_H_
#define IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "qcloud_iot_llsync.h"

// ------------------------------------------------------------------------------
// define
// ------------------------------------------------------------------------------

// 1 byte type + 2 bytes payload-length
#define BLE_QIOT_EVENT_FIXED_HEADER_LEN (3)
// the bit 15 - 14 is slice flag, bit 13 - 0 is tlv length
#define BLE_QIOT_IS_SLICE_PACKAGE(_C) ((_C)&0XC0)
#define BLE_QIOT_IS_SLICE_HEADER(_C)  (((_C)&0XC0) == 0X40)
#define BLE_QIOT_IS_SLICE_BODY(_C)    (((_C)&0XC0) == 0X80)
#define BLE_QIOT_IS_SLICE_TAIL(_C)    (((_C)&0XC0) == 0XC0)

#define BLE_QIOT_STRING_TYPE_LEN     2                               // string/struct type length
#define BLE_QIOT_MIN_STRING_TYPE_LEN (BLE_QIOT_STRING_TYPE_LEN + 1)  // at least 2 bytes length and 1 byte payload
#define BLE_QIOT_NOT_SUPPORT_WARN    " not support, please check the data template"

#define BLE_QIOT_CONTROL_DATA_TYPE          (0x00)
#define BLE_QIOT_GET_STATUS_REPLY_DATA_TYPE (0x22)

#define BLE_QIOT_GET_STATUS_REPLY_HEADER_LEN (4)
#define BLE_QIOT_DATA_FIXED_HEADER_LEN       (3)

#define BLE_LLSYNC_CHARACTERISTIC_UUID_DEVICE_INFO (0xffe1)
#define BLE_LLSYNC_CHARACTERISTIC_UUID_DATA        (0xffe2)
#define BLE_LLSYNC_CHARACTERISTIC_UUID_EVENT       (0xffe3)
#define BLE_LLSYNC_CHARACTERISTIC_UUID_OTA         (0xffe4)
#define BLE_LLSYNC_CHARACTERISTIC_UUID_SUBDEV_CTRL (0xffe5)
#define BLE_LLSYNC_CHARACTERISTIC_UUID_SUBDEV_RPT  (0xffe6)

// tlv header define, bit 7 - 5 is type, bit 4 - 0 depends on type of data template
#define BLE_QIOT_PARSE_TLV_HEAD_TYPE(_C) (((_C)&0XFF) >> 5)
#define BLE_QIOT_PARSE_TLV_HEAD_ID(_C)   ((_C)&0X1F)

#define BLE_QIOT_GET_OTA_REQUEST_HEADER_LEN 3  // the ota request header len
#define BLE_QIOT_OTA_DATA_HEADER_LEN        3  // the ota data header len

// ota feature
#define BLE_QIOT_OTA_ENABLE        (1 << 0)
#define BLE_QIOT_OTA_RESUME_ENABLE (1 << 1)

// ota file valid result
#define BLE_QIOT_OTA_VALID_SUCCESS (1 << 7)
#define BLE_QIOT_OTA_VALID_FAIL    (0 << 7)

#define BLE_QIOT_OTA_MAX_VERSION_STR (32)  // max ota version length
#define BLE_QIOT_OTA_PAGE_VALID_VAL  0x5A  // ota info valid flag

#define BLE_QIOT_OTA_FIRST_TIMEOUT   (1)
#define BLE_QIOT_OTA_MAX_RETRY_COUNT 5  // disconnect if retry times more than BLE_QIOT_OTA_MAX_RETRY_COUNT

// ota control bits
#define BLE_QIOT_OTA_REQUEST_BIT     (1 << 0)
#define BLE_QIOT_OTA_RECV_END_BIT    (1 << 1)
#define BLE_QIOT_OTA_RECV_DATA_BIT   (1 << 2)
#define BLE_QIOT_OTA_FIRST_RETRY_BIT (1 << 3)
#define BLE_QIOT_OTA_DO_VALID_BIT    (1 << 4)
#define BLE_QIOT_OTA_HAVE_DATA_BIT   (1 << 5)

// the reason of ota file error
enum {
    BLE_QIOT_OTA_CRC_ERROR        = 0,
    BLE_QIOT_OTA_READ_FLASH_ERROR = 1,
    BLE_QIOT_OTA_FILE_ERROR       = 2,
};

// ota data type
typedef enum {
    BLE_QIOT_OTA_MSG_REQUEST = 0,
    BLE_QIOT_OTA_MSG_DATA    = 1,
    BLE_QIOT_OTA_MSG_END     = 2,
    BLE_QIOT_OTA_MSG_BUTT,
}BLELLsyncOTAMsg;

typedef enum {
    BLE_QIOT_EVENT_NO_SLICE   = 0,
    BLE_QIOT_EVENT_SLICE_HEAD = 1,
    BLE_QIOT_EVENT_SLICE_BODY = 2,
    BLE_QIOT_EVENT_SLICE_FOOT = 3,
} BLELLsyncSliceType;

typedef enum {
    BLE_QIOT_REPLY_SUCCESS = 0,
    BLE_QIOT_REPLY_FAIL,
    BLE_QIOT_REPLY_DATA_ERR,
    BLE_QIOT_REPLY_BUTT,
} BLELLsycReplyResult;

// define message type that from server to device
typedef enum {
    BLE_QIOT_DATA_DOWN_REPORT_REPLY = 0,
    BLE_QIOT_DATA_DOWN_CONTROL,
    BLE_QIOT_DATA_DOWN_GET_STATUS_REPLY,
    BLE_QIOT_DATA_DOWN_ACTION,
    BLE_QIOT_DATA_DOWN_EVENT_REPLY,
} BLELLsyncDataDownType;

// define message type that from device to server
typedef enum {
    BLE_QIOT_EVENT_UP_PROPERTY_REPORT = 0,
    BLE_QIOT_EVENT_UP_CONTROL_REPLY,
    BLE_QIOT_EVENT_UP_GET_STATUS,
    BLE_QIOT_EVENT_UP_EVENT_POST,
    BLE_QIOT_EVENT_UP_ACTION_REPLY,
    BLE_QIOT_EVENT_UP_BIND_SIGN_RET,
    BLE_QIOT_EVENT_UP_CONN_SIGN_RET,
    BLE_QIOT_EVENT_UP_UNBIND_SIGN_RET,
    BLE_QIOT_EVENT_UP_REPORT_MTU,
    BLE_QIOT_EVENT_UP_REPLY_OTA_REPORT,
    BLE_QIOT_EVENT_UP_REPLY_OTA_DATA,
    BLE_QIOT_EVENT_UP_REPORT_CHECK_RESULT,
    BLE_QIOT_EVENT_UP_SYNC_MTU,
    BLE_QIOT_EVENT_UP_SYNC_WAIT_TIME,
    BLE_QIOT_EVENT_UP_DYNREG_SIGN,
    BLE_QIOT_EVENT_UP_WIFI_MODE = 0xE0,
    BLE_QIOT_EVENT_UP_WIFI_INFO,
    BLE_QIOT_EVENT_UP_WIFI_CONNECT,
    BLE_QIOT_EVENT_UP_WIFI_TOKEN,
    BLE_QIOT_EVENT_UP_WIFI_LOG,
    BLE_QIOT_EVENT_UP_BUTT,
} BLELLsyncUpLinkType;

typedef enum {
    BLE_QIOT_EFFECT_REQUEST = 0,
    BLE_QIOT_EFFECT_REPLY,
    BLE_QIOT_EFFECT_BUTT,
} BLELLsyncEffectType;

typedef enum {
    E_REPORT_DEVNAME = 0,
    E_REPORT_DEVINFO = 1,
} BLELLsyncReportDevType;

typedef enum {
    BLE_WIFI_MODE_NULL = 0,  // invalid mode
    BLE_WIFI_MODE_STA  = 1,  // station
    BLE_WIFI_MODE_AP   = 2,  // ap
} BLELLsyncWifiModeType;

typedef enum {
    BLE_WIFI_STATE_CONNECT = 0,  // wifi connect
    BLE_WIFI_STATE_OTHER   = 1,  // other state
} BLELLsyncWifiState;

typedef enum {
    E_BLE_SECURE_BIND_CONFIRM = 0,
    E_BLE_SECURE_BIND_REJECT  = 1,
} BLELLsyncSecureBindState;

typedef struct {
    uint16_t    char_uuid;
    uint8_t     type;
    uint8_t     length_flag;
    uint8_t    *header;
    uint8_t     header_len;
    const char *buf;
    uint16_t    buf_len;
} LLsyncUpLinkData;

#define DEFAULT_LLSYNC_UPLINK_DATA                                                                      \
    {                                                                                                   \
        .char_uuid = BLE_LLSYNC_CHARACTERISTIC_UUID_EVENT, .length_flag = 0, .type = 0, .header = NULL, \
        .header_len = 0, .buf = NULL, .buf_len = 0,                                                     \
    }

int ble_uplink_notify2(LLsyncUpLinkData *uplink_data);

int ble_uplink_report_device_info(uint8_t type);

int ble_uplink_sync_mtu(uint16_t att_mtu);

#ifdef __cplusplus
}
#endif
#endif  // IOT_HUB_DEVICE_C_SDK_COMPONENT_BLE_LLSYNC_INC_BLE_LLSYNC_SERVICE_H_
